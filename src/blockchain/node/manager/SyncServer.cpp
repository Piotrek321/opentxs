// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                            // IWYU pragma: associated
#include "1_Internal.hpp"                          // IWYU pragma: associated
#include "blockchain/node/manager/SyncServer.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>

#include "internal/api/session/Endpoints.hpp"
#include "internal/blockchain/database/Sync.hpp"
#include "internal/blockchain/node/HeaderOracle.hpp"
#include "internal/blockchain/node/filteroracle/FilterOracle.hpp"
#include "internal/network/p2p/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/Signals.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Header.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/GCS.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Header.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/network/p2p/Base.hpp"
#include "opentxs/network/p2p/Data.hpp"
#include "opentxs/network/p2p/MessageType.hpp"
#include "opentxs/network/p2p/Request.hpp"
#include "opentxs/network/p2p/State.hpp"
#include "opentxs/network/p2p/Types.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/WorkType.hpp"

namespace opentxs::blockchain::node::base
{
class SyncServer;

auto SyncServer::Tip() const noexcept -> block::Position
{
    return db_.SyncTip();
}
auto SyncServer::NextBatch() noexcept -> BatchType
{
    return allocate_batch(type_);
}
auto SyncServer::Shutdown() noexcept -> std::shared_future<void>
{
    {
        auto lock = Lock{zmq_lock_};
        zmq_running_ = false;
    }

    if (zmq_thread_.joinable()) { zmq_thread_.join(); }

    return signal_shutdown();
}

SyncServer::SyncServer(
    const api::Session& api,
    database::Sync& db,
    const node::HeaderOracle& header,
    const node::internal::FilterOracle& filter,
    const node::internal::Manager& node,
    const blockchain::Type chain,
    const cfilter::Type type,
    const UnallocatedCString& shutdown,
    const UnallocatedCString& publishEndpoint) noexcept
    : SyncDM(
          [&] { return db.SyncTip(); }(),
          [&] {
              auto promise = std::promise<int>{};
              promise.set_value(0);

              return Finished{promise.get_future()};
          }(),
          "sync server",
          2000,
          1000)
    , SyncWorker(api, 20ms)
    , db_(db)
    , header_(header)
    , filter_(filter)
    , node_(node)
    , chain_(chain)
    , type_(type)
    , linger_(0)
    , endpoint_(publishEndpoint)
    , socket_(::zmq_socket(api_.Network().ZeroMQ(), ZMQ_PAIR), ::zmq_close)
    , zmq_lock_()
    , zmq_running_(true)
    , zmq_thread_(&SyncServer::zmq_thread, this)
{
    init_executor(
        {shutdown,
         UnallocatedCString{
             api_.Endpoints().Internal().BlockchainFilterUpdated(chain_)}});
    ::zmq_setsockopt(socket_.get(), ZMQ_LINGER, &linger_, sizeof(linger_));
    ::zmq_connect(socket_.get(), endpoint_.c_str());
}

SyncServer::~SyncServer()
{
    try {
        Shutdown().get();
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
        // TODO MT-34 improve
    }
}

auto SyncServer::batch_ready() const noexcept -> void { trigger(); }
auto SyncServer::batch_size(const std::size_t in) noexcept -> std::size_t
{
    if (in < 10) {

        return 1;
    } else if (in < 100) {

        return 10;
    } else if (in < 1000) {

        return 100;
    } else {

        return 1000;
    }
}
auto SyncServer::check_task(TaskType&) const noexcept -> void {}

auto SyncServer::hello(const Lock&, const block::Position& incoming)
    const noexcept
{
    // TODO use known() and Ancestors() instead
    auto [parent, best] = header_.CommonParent(incoming);
    if ((0 == parent.height_) && (1000 < incoming.height_)) {
        const auto height = std::min(incoming.height_ - 1000, best.height_);
        parent = {height, header_.BestHash(height)};
    }
    const auto needSync = incoming != best;
    auto state = network::p2p::State{chain_, std::move(best)};

    return std::make_tuple(needSync, parent, std::move(state));
}

auto SyncServer::trigger_state_machine() const noexcept -> void { trigger(); }

auto SyncServer::update_tip(const Position& position, const int&) const noexcept
    -> void
{
    auto saved = db_.SetSyncTip(position);

    OT_ASSERT(saved);

    LogDetail()(print(chain_))(" sync data updated to height ")(
        position.height_)
        .Flush();
}

auto SyncServer::download() noexcept -> void
{
    auto work = NextBatch();

    for (const auto& task : work.data_) {
        // TODO allocator
        task->download(filter_.LoadFilter(type_, task->position_.hash_, {}));
    }
}
auto SyncServer::process_position(const zmq::Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(body.size() > 3);

    const auto type = body.at(1).as<cfilter::Type>();

    if (type != type_) { return; }

    process_position(
        Position{body.at(2).as<block::Height>(), body.at(3).Bytes()});
}
auto SyncServer::process_position(const Position& pos) noexcept -> void
{
    LogTrace()(OT_PRETTY_CLASS())(__func__)(": processing block ")(pos).Flush();

    try {
        auto current = known();
        auto hashes = header_.Ancestors(current, pos, 2000);
        LogTrace()(OT_PRETTY_CLASS())(__func__)(
            ": current position best known position is block ")(current)
            .Flush();

        OT_ASSERT(0 < hashes.size());

        if (1 == hashes.size()) {
            LogTrace()(OT_PRETTY_CLASS())(__func__)(
                ": current position matches incoming block ")(pos)
                .Flush();

            return;
        }

        auto prior = Previous{std::nullopt};
        {
            auto& first = hashes.front();
            auto promise = std::promise<int>{};
            promise.set_value(0);
            prior.emplace(std::move(first), promise.get_future());
        }
        hashes.erase(hashes.begin());
        {
            const auto& first = hashes.front();
            const auto& last = hashes.back();

            if (first.height_ <= current.height_) {
                LogTrace()(OT_PRETTY_CLASS())(__func__)(": reorg detected")
                    .Flush();
            }

            LogTrace()(OT_PRETTY_CLASS())(__func__)(
                ": scheduling download starting from block ")(
                first)(" until block ")(last)
                .Flush();
        }
        update_position(std::move(hashes), type_, std::move(prior));
    } catch (...) {
    }
}
auto SyncServer::process_zmq(const Lock& lock) noexcept -> void
{
    network::zeromq::Message incoming{};
    OTSocket::receive_message(lock, socket_.get(), incoming);

    namespace bcsync = network::p2p;
    const auto base = api_.Factory().BlockchainSyncMessage(incoming);

    if (auto type = base->Type(); type != bcsync::MessageType::sync_request) {
        LogError()(OT_PRETTY_CLASS())(__func__)(
            ": Invalid or unsupported message type ")(opentxs::print(type))
            .Flush();

        return;
    }

    try {
        const auto& request = base->asRequest();
        const auto& state = std::find_if(
            request.State().begin(),
            request.State().end(),
            [&chain = chain_](const auto& p2pState) {
                return p2pState.Chain() == chain;
            });
        if (state == request.State().end())
            throw std::runtime_error{"No matching chains"};

        const auto& position = state->Position();
        auto [needSync, parent, data] = hello(lock, position);
        const auto& [height, hash] = parent;
        auto reply = factory::BlockchainSyncData(
            WorkType::P2PBlockchainSyncReply, std::move(data), {}, {});
        auto send{true};

        if (needSync) { send = db_.LoadSync(height, reply); }

        auto out = network::zeromq::reply_to_message(incoming);

        if (send && reply.Serialize(out)) {
            OTSocket::send_message(lock, socket_.get(), std::move(out));
        }
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(__func__)(": ")(e.what()).Flush();
    }
}
auto SyncServer::queue_processing(DownloadedData&& data) noexcept -> void
{
    if (data.empty()) { return; }

    const auto& tip = data.back();
    auto items = UnallocatedVector<network::p2p::Block>{};
    auto previousFilterHeader = api_.Factory().Data();

    for (const auto& task : data) {
        try {
            const auto pHeader =
                header_.Internal().LoadBitcoinHeader(task->position_.hash_);

            if (false == bool(pHeader)) {
                throw std::runtime_error(
                    UnallocatedCString{"failed to load block header "} +
                    task->position_.hash_.asHex());
            }

            const auto& header = *pHeader;

            if (previousFilterHeader->empty()) {
                previousFilterHeader =
                    filter_.LoadFilterHeader(type_, header.ParentHash());

                if (previousFilterHeader->empty()) {
                    throw std::runtime_error(
                        UnallocatedCString{
                            "failed to previous filter header for "
                            "block  "} +
                        task->position_.hash_.asHex());
                }
            }

            const auto& cfilter = task->data_.get();

            if (false == cfilter.IsValid()) {
                throw std::runtime_error(
                    UnallocatedCString{"failed to load gcs for block "} +
                    task->position_.hash_.asHex());
            }

            const auto headerBytes = header.Encode();
            Space filterBytes{};
            cfilter.Compressed(writer(filterBytes));

            items.emplace_back(
                chain_,
                task->position_.height_,
                type_,
                cfilter.ElementCount(),
                headerBytes->Bytes(),
                reader(filterBytes));
            task->process(1);
        } catch (const std::exception& e) {
            LogError()(OT_PRETTY_CLASS())(__func__)(": ")(e.what()).Flush();
            task->redownload();
            break;
        }
    }

    if (previousFilterHeader->empty() || (items.empty())) {
        LogError()(OT_PRETTY_CLASS())(__func__)(": missing data").Flush();

        return;
    }

    const auto& pos = tip->position_;
    const auto stored = db_.StoreSync(pos, items);

    if (false == stored) { OT_FAIL; }

    const auto msg = factory::BlockchainSyncData(
        WorkType::P2PBlockchainNewBlock,
        {chain_, pos},
        std::move(items),
        previousFilterHeader->Bytes());
    auto work = network::zeromq::Message{};

    if (msg.Serialize(work) && zmq_running_) {
        // NOTE the appropriate lock is already being held in the pipeline
        // function
        auto dummy = std::mutex{};
        auto lock = Lock{dummy};
        OTSocket::send_message(lock, socket_.get(), std::move(work));
    }
}
auto SyncServer::zmq_thread() noexcept -> void
{
    Signals::Block();
    UnallocatedVector<::zmq_pollitem_t> poll{};
    auto& pollItem = poll.emplace_back();
    pollItem.socket = socket_.get();
    pollItem.events = ZMQ_POLLIN;

    OT_ASSERT(std::numeric_limits<int>::max() >= poll.size());
    while (zmq_running_) {
        constexpr auto timeout = 250ms;
        const auto events = ::zmq_poll(
            poll.data(), static_cast<int>(poll.size()), timeout.count());

        if (0 > events) {
            const auto error = ::zmq_errno();
            LogError()(OT_PRETTY_CLASS())(__func__)(": ")(::zmq_strerror(error))
                .Flush();

            continue;
        } else if (0 == events) {

            continue;
        }

        auto lock = Lock{zmq_lock_};

        for (const auto& item : poll) {
            if (ZMQ_POLLIN != item.revents) { continue; }

            process_zmq(lock);
        }
    }

    ::zmq_disconnect(socket_.get(), endpoint_.c_str());
}

auto SyncServer::pipeline(zmq::Message&& in) -> void
{
    if (!running_.load()) { return; }

    const auto body = in.Body();

    OT_ASSERT(1 <= body.size());

    using Work = Work;
    const auto work = body.at(0).as<Work>();
    auto lock = Lock{zmq_lock_};

    switch (work) {
        case Work::shutdown: {
            protect_shutdown([this] { shut_down(); });
        } break;
        case Work::heartbeat: {
            if (dm_enabled()) { process_position(filter_.Tip(type_)); }

            run_if_enabled();
        } break;
        case Work::filter: {
            process_position(in);
            run_if_enabled();
        } break;
        case Work::statemachine: {
            download();
            run_if_enabled();
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto SyncServer::state_machine() noexcept -> bool
{
    return SyncDM::state_machine();
}

auto SyncServer::shut_down() noexcept -> void
{
    close_pipeline();
    // TODO MT-34 investigate what other actions might be needed
}

}  // namespace opentxs::blockchain::node::base
