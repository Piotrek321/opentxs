// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <boost/smart_ptr/detail/operator_bool.hpp>

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/blockoracle/BlockFetcher.hpp"  // IWYU pragma: associated

#include <boost/smart_ptr/make_shared.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <chrono>
#include <exception>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "blockchain/node/blockoracle/BlockBatch.hpp"
#include "internal/api/session/Endpoints.hpp"
#include "internal/blockchain/database/Block.hpp"
#include "internal/blockchain/node/Endpoints.hpp"
#include "internal/blockchain/node/blockoracle/BlockBatch.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/socket/Pipeline.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/P0330.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/core/FixedByteArray.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Types.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/ScopeGuard.hpp"
#include "util/Work.hpp"

namespace opentxs::blockchain::node::blockoracle
{
auto print(BlockFetcherJob job) noexcept -> std::string_view
{
    using namespace std::literals;

    try {
        using Job = BlockFetcherJob;
        static const auto map = Map<Job, std::string_view>{
            {Job::shutdown, "shutdown"sv},
            {Job::header, "header"sv},
            {Job::reorg, "reorg"sv},
            {Job::block_received, "block_received"sv},
            {Job::batch_finished, "batch_finished"sv},
            {Job::init, "init"sv},
            {Job::statemachine, "statemachine"sv},
        };

        return map.at(job);
    } catch (...) {
        LogError()(__FUNCTION__)("invalid BlockFetcherJob: ")(
            static_cast<OTZMQWorkType>(job))
            .Flush();

        OT_FAIL;
    }
}
}  // namespace opentxs::blockchain::node::blockoracle

namespace opentxs::blockchain::node::blockoracle
{
BlockFetcher::Imp::Imp(
    const api::Session& api,
    const Endpoints& endpoints,
    const HeaderOracle& header,
    database::Block& db,
    blockchain::Type chain,
    std::size_t peerTarget,
    network::zeromq::BatchID batch,
    allocator_type alloc) noexcept
    : Actor(
          api,
          LogTrace(),
          [&] {
              using opentxs::blockchain::print;
              auto out = CString{print(chain), alloc};
              out.append(" block fetcher");

              return out;
          }(),
          0ms,
          batch,
          alloc,
          [&] {
              using Dir = network::zeromq::socket::Direction;
              auto out = network::zeromq::EndpointArgs{alloc};
              out.emplace_back(
                  CString{api.Endpoints().BlockchainReorg(), alloc},
                  Dir::Connect);

              return out;
          }(),
          {},
          {},
          [&] {
              auto out = Vector<network::zeromq::SocketData>{alloc};
              using Socket = network::zeromq::socket::Type;
              using Args = network::zeromq::EndpointArgs;
              using Dir = network::zeromq::socket::Direction;
              out.emplace_back(std::make_pair<Socket, Args>(
                  Socket::Publish,
                  {
                      {{endpoints.block_fetcher_job_ready_, alloc}, Dir::Bind},
                  }));
              out.emplace_back(std::make_pair<Socket, Args>(
                  Socket::Publish,
                  {
                      {CString{
                           api.Endpoints().Internal().BlockchainBlockUpdated(
                               chain),
                           alloc},
                       Dir::Bind},
                  }));

              return out;
          }())
    , api_(api)
    , header_oracle_(header)
    , db_(db)
    , job_ready_(pipeline_.Internal().ExtraSocket(0))
    , tip_updated_(pipeline_.Internal().ExtraSocket(1))
    , chain_(chain)
    , peer_target_(peerTarget)
    , data_(get_allocator())
    , self_()
{
}

auto BlockFetcher::Imp::broadcast_tip(const block::Position& tip) noexcept
    -> void
{
    log_(OT_PRETTY_CLASS())(name_)(": block tip updated to ")(tip).Flush();
    const auto saved = db_.SetBlockTip(tip);

    OT_ASSERT(saved);

    tip_updated_.SendDeferred([&] {
        auto msg = MakeWork(OT_ZMQ_NEW_FULL_BLOCK_SIGNAL);
        msg.AddFrame(tip.height_);
        msg.AddFrame(tip.hash_);

        return msg;
    }());
}

auto BlockFetcher::Imp::do_shutdown() noexcept -> void { self_.reset(); }

auto BlockFetcher::Imp::do_startup() noexcept -> void
{
    {
        auto handle = data_.lock();
        auto& tip = handle->tip_;
        tip = db_.BlockTip();
        const auto original = tip;

        OT_ASSERT(0 <= tip.height_);

        if (auto r = header_oracle_.CalculateReorg(tip); false == r.empty()) {
            const auto& last = r.back();

            OT_ASSERT(0 < last.height_);

            const auto header = header_oracle_.LoadHeader(last.hash_);

            OT_ASSERT(header);

            tip = {last.height_ - 1, header->ParentHash()};
        }

        while (tip.height_ > 0) {
            if (auto block = db_.BlockLoadBitcoin(tip.hash_); block) {

                break;
            } else {

                tip = header_oracle_.GetPosition(tip.height_ - 1);
            }
        }

        if (original != tip) { broadcast_tip(tip); }

        log_(OT_PRETTY_CLASS())(": best downloaded full block is ")(tip)
            .Flush();
    }

    do_work();
}

auto BlockFetcher::Imp::erase_obsolete(
    const block::Position& after,
    Data& data) noexcept -> void
{
    auto& blocks = data.blocks_;

    if (blocks.empty()) { return; }

    for (auto i = blocks.lower_bound(after.height_), stop = blocks.end();
         i != stop;) {
        auto& [height, val] = *i;
        auto& [hash, job, status] = val;

        if ((height == after.height_) && (hash == after.hash_)) {
            ++i;
        } else {
            if (job.has_value()) {
                data.job_index_.at(*job).erase(hash);
            } else {
                OT_ASSERT(0_uz < data.queue_);

                --data.queue_;
            }

            i = blocks.erase(i);
        }
    }
}

auto BlockFetcher::Imp::GetJob(allocator_type pmr) const noexcept
    -> internal::BlockBatch
{
    OT_ASSERT(self_);

    using Imp = internal::BlockBatch::Imp;
    auto alloc = alloc::PMR<Imp>{pmr};
    auto* imp = alloc.allocate(1_uz);
    const auto id = download::next_job();
    auto hashes = [&] {
        // TODO define max in Params
        static constexpr auto max = 50000_uz;
        static constexpr auto min = 10_uz;
        auto handle = data_.lock();
        auto& data = *handle;
        auto& queue = data.queue_;
        auto& blocks = data.blocks_;
        const auto count = download::batch_size(queue, peer_target_, max, min);

        OT_ASSERT(blocks.size() >= count);
        OT_ASSERT(count <= queue);

        auto& index = data.job_index_[id];
        auto out = Vector<block::Hash>{pmr};
        out.reserve(count);
        auto i = blocks.begin();

        while (out.size() < count) {
            auto cur = i++;
            auto& [height, val] = *cur;
            auto& [hash, job, status] = val;

            if (Status::pending == status) {
                --queue;
                job = id;
                status = Status::downloading;
                --data.queue_;
                out.emplace_back(hash);
                index.emplace(hash, cur);
            }
        }

        return out;
    }();

    if (hashes.empty()) {

        return {};
    } else {
        auto download = [me = self_, id](const auto bytes) {
            me->pipeline_.Push([bytes, id] {
                auto msg = MakeWork(Work::block_received);
                msg.AddFrame(id);
                msg.AddFrame(bytes.data(), bytes.size());

                return msg;
            }());
        };
        auto finished = [me = self_, id] {
            me->pipeline_.Push([id] {
                auto msg = MakeWork(Work::batch_finished);
                msg.AddFrame(id);

                return msg;
            }());
        };
        alloc.construct(
            imp,
            id,
            std::move(hashes),
            download,
            std::make_shared<ScopeGuard>(finished));

        return imp;
    }
}

auto BlockFetcher::Imp::Init(boost::shared_ptr<Imp> self) noexcept -> void
{
    self_ = std::move(self);
    signal_startup(self_);
}

auto BlockFetcher::Imp::pipeline(const Work work, Message&& msg) noexcept
    -> void
{
    switch (work) {
        case Work::shutdown: {
            shutdown_actor();
        } break;
        case Work::block_received: {
            process_block_received(std::move(msg));
        } break;
        case Work::batch_finished: {
            process_batch_finished(std::move(msg));
        } break;
        case Work::reorg: {
            process_reorg(std::move(msg));
        } break;
        case Work::init: {
            do_init();
        } break;
        case Work::header:
        case Work::statemachine: {
            do_work();
        } break;
        default: {
            LogError()(OT_PRETTY_CLASS())(name_)(": unhandled message type ")(
                static_cast<OTZMQWorkType>(work))
                .Flush();

            OT_FAIL;
        }
    }
}

auto BlockFetcher::Imp::process_batch_finished(Message&& msg) noexcept -> void
{
    auto body = msg.Body();

    OT_ASSERT(1_uz < body.size());

    const auto id = body.at(1).as<download::JobID>();
    auto handle = data_.lock();
    auto& index = handle->job_index_;

    if (auto i = index.find(id); index.end() != i) {
        for (auto& [block, j] : i->second) {
            auto& [height, data] = *j;
            auto& [hash, job, status] = data;
            job = std::nullopt;

            if (Status::downloading == status) { status = Status::pending; }
        }

        index.erase(i);
    }

    update_tip(*handle);
}

auto BlockFetcher::Imp::process_block_received(Message&& msg) noexcept -> void
{
    auto body = msg.Body();

    OT_ASSERT(2_uz < body.size());

    const auto id = body.at(1).as<download::JobID>();
    auto pBlock = api_.Factory().BitcoinBlock(chain_, body.at(2).Bytes());

    if (false == pBlock.operator bool()) {
        log_(OT_PRETTY_CLASS())(name_)(": received invalid block").Flush();

        return;
    }

    const auto& block = *pBlock;
    auto handle = data_.lock();
    auto& batch = handle->job_index_.at(id);
    const auto& hash = block.ID();

    if (auto i = batch.find(hash); batch.end() != i) {
        const auto& [key, value] = *i;
        auto& [h, job, status] = value->second;
        const auto saved = db_.BlockStore(block);

        OT_ASSERT(saved);

        status = Status::success;
        update_tip(*handle);
    } else {
        log_(OT_PRETTY_CLASS())(name_)(": received block ")
            .asHex(hash)(" but it is not part of batch ")(id)
            .Flush();
    }
}

auto BlockFetcher::Imp::process_reorg(Message&& msg) noexcept -> void
{
    const auto body = msg.Body();

    OT_ASSERT(5 < body.size());

    if (body.at(1).as<decltype(chain_)>() != chain_) { return; }

    auto ancestor =
        block::Position{body.at(3).as<block::Height>(), body.at(2).Bytes()};

    {
        auto handle = data_.lock();
        auto& tip = handle->tip_;
        const auto original{tip};

        if (tip.height_ > ancestor.height_) {
            tip = std::move(ancestor);
        } else if ((tip.height_ == ancestor.height_) && (tip != ancestor)) {
            OT_ASSERT(0 < tip.height_);

            tip = header_oracle_.GetPosition(tip.height_ - 1);
        }

        if (original != tip) { broadcast_tip(tip); }

        erase_obsolete(tip, *handle);
    }

    do_work();
}

auto BlockFetcher::Imp::Shutdown() noexcept -> void
{
    // WARNING this function must never be called from with this class's
    // Actor::worker function or else a deadlock will occur. Shutdown must only
    // be called by a different Actor.
    auto lock = std::unique_lock<std::timed_mutex>{reorg_lock_};
    signal_shutdown();
}

auto BlockFetcher::Imp::update_tip(Data& data) noexcept -> void
{
    auto& tip = data.tip_;
    auto& blocks = data.blocks_;
    auto expected = tip.height_;
    auto newTip = std::optional<block::Position>{std::nullopt};
    auto erase = [&] {
        for (auto i = blocks.begin(), end = blocks.end(); i != end; ++i) {
            const auto& [height, val] = *i;
            const auto& [hash, job, status] = val;

            OT_ASSERT(++expected == height);

            if (Status::success == status) {
                OT_ASSERT(false == job.has_value());

                newTip.emplace(height, hash);
                log_(OT_PRETTY_CLASS())(name_)(": block ")(
                    height)(" successfully downloaded")
                    .Flush();
            } else {
                log_(OT_PRETTY_CLASS())(name_)(": block ")(
                    height)(" download still in progress")
                    .Flush();

                return i;
            }
        }

        return blocks.end();
    }();
    blocks.erase(blocks.begin(), erase);

    if (newTip.has_value()) {
        tip = *newTip;
        broadcast_tip(tip);
    }
}

auto BlockFetcher::Imp::work() noexcept -> bool
{
    log_(OT_PRETTY_CLASS())(name_)(": checking for new blocks").Flush();
    auto handle = data_.lock();
    auto& queue = handle->queue_;
    auto& blocks = handle->blocks_;
    auto& tip = handle->tip_;
    auto start = [&] {
        if (blocks.empty()) {

            return tip.height_;
        } else {

            return blocks.crbegin()->first;
        }
    }();
    log_(OT_PRETTY_CLASS())(name_)(": have blocks up to ")(start).Flush();
    auto post = ScopeGuard{[&] {
        if (0_uz < queue) {
            log_(OT_PRETTY_CLASS())(name_)(
                ": notifying listeners about new block download jobs")
                .Flush();
            job_ready_.SendDeferred(MakeWork(OT_ZMQ_BLOCK_FETCH_JOB_AVAILABLE));
        }
    }};
    auto newBlocks = [&] {
        auto out = Vector<block::Position>{get_allocator()};
        auto hashes =
            header_oracle_.BestHashes(start + 1, 0_uz, get_allocator());
        out.reserve(hashes.size());

        for (auto& hash : hashes) {
            ++start;
            out.emplace_back(start, std::move(hash));
        }

        return out;
    }();

    if (newBlocks.empty()) {
        log_(OT_PRETTY_CLASS())(name_)(
            ": block tip caught up to block header tip")
            .Flush();

        return false;
    }

    erase_obsolete(newBlocks.front(), *handle);

    for (auto& pos : newBlocks) {
        const auto status = [&] {
            if (db_.BlockExists(pos.hash_)) {
                log_(OT_PRETTY_CLASS())(name_)(": block ")(
                    pos)(" already downloaded")
                    .Flush();

                return Status::success;
            } else {
                log_(OT_PRETTY_CLASS())(name_)(": adding block ")(
                    pos)(" to queue")
                    .Flush();
                ++queue;

                return Status::pending;
            }
        }();
        blocks.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(pos.height_),
            std::forward_as_tuple(std::move(pos.hash_), std::nullopt, status));
    }

    update_tip(*handle);

    return false;
}

BlockFetcher::Imp::~Imp() { do_shutdown(); }
}  // namespace opentxs::blockchain::node::blockoracle

namespace opentxs::blockchain::node::blockoracle
{
BlockFetcher::BlockFetcher(
    const api::Session& api,
    const Endpoints& endpoints,
    const HeaderOracle& header,
    database::Block& db,
    blockchain::Type chain,
    std::size_t peerTarget) noexcept
    : imp_([&] {
        using ReturnType = BlockFetcher::Imp;
        const auto& asio = api.Network().ZeroMQ().Internal();
        const auto batchID = asio.PreallocateBatch();
        // TODO the version of libc++ present in android ndk 23.0.7599858
        // has a broken std::allocate_shared function so we're using
        // boost::shared_ptr instead of std::shared_ptr

        return boost::allocate_shared<ReturnType>(
            alloc::PMR<ReturnType>{asio.Alloc(batchID)},
            api,
            endpoints,
            header,
            db,
            chain,
            peerTarget,
            batchID);
    }())
{
    OT_ASSERT(imp_);
}

BlockFetcher::BlockFetcher(const BlockFetcher& rhs) noexcept
    : imp_(rhs.imp_)
{
    OT_ASSERT(imp_);
}

BlockFetcher::BlockFetcher(BlockFetcher&& rhs) noexcept
    : imp_(std::move(rhs.imp_))
{
    OT_ASSERT(imp_);
}

auto BlockFetcher::operator=(const BlockFetcher& rhs) noexcept -> BlockFetcher&
{
    if (this != std::addressof(rhs)) { imp_ = rhs.imp_; }

    return *this;
}

auto BlockFetcher::operator=(BlockFetcher&& rhs) noexcept -> BlockFetcher&
{
    using std::swap;
    swap(imp_, rhs.imp_);

    return *this;
}

auto BlockFetcher::get_allocator() const noexcept -> allocator_type
{
    return imp_->get_allocator();
}

auto BlockFetcher::GetJob(allocator_type alloc) const noexcept
    -> internal::BlockBatch
{
    return imp_->GetJob(alloc);
}

auto BlockFetcher::Shutdown() noexcept -> void
{
    if (imp_) { imp_->Shutdown(); }
}

auto BlockFetcher::Start() noexcept -> void { imp_->Init(imp_); }

BlockFetcher::~BlockFetcher() { Shutdown(); }
}  // namespace opentxs::blockchain::node::blockoracle
