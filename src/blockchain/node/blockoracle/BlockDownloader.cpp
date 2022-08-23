// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/blockoracle/BlockDownloader.hpp"  // IWYU pragma: associated

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "internal/api/session/Endpoints.hpp"
#include "internal/blockchain/Params.hpp"
#include "internal/blockchain/database/Block.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/blockchain/node/Types.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::blockchain::node::blockoracle
{
BlockDownloader::BlockDownloader(
    const api::Session& api,
    database::Block& db,
    const node::HeaderOracle& header,
    const internal::Manager& node,
    const blockchain::Type chain,
    const std::string_view shutdown) noexcept
    : BlockDMBlock(
          [&] { return db.BlockTip(); }(),
          [&] {
              auto promise = std::promise<int>{};
              promise.set_value(0);

              return Finished{promise.get_future()};
          }(),
          "block",
          2000,
          1000)
    , BlockWorkerBlock(api, "BlockDownloader")
    , db_(db)
    , header_(header)
    , node_(node)
    , chain_(chain)
    , socket_(api_.Network().ZeroMQ().PublishSocket())
    , last_job_{}
{
    init_executor(
        {UnallocatedCString{shutdown},
         UnallocatedCString{api_.Endpoints().BlockchainReorg()}});
    auto zmq = socket_->Start(
        api_.Endpoints().Internal().BlockchainBlockUpdated(chain_).data());

    start();

    OT_ASSERT(zmq);
}

auto BlockDownloader::to_string(Work value) const -> const std::string&
{
    static const auto Map = std::map<Work, std::string>{
        {Work::shutdown, "shutdown"},
        {Work::block, "block"},
        {Work::reorg, "reorg"},
        {Work::heartbeat, "heartbeat"},
        {Work::statemachine, "statemachine"},
    };
    try {
        return Map.at(value);
    } catch (...) {
        LogError()(__FUNCTION__)("invalid BlockDownloader job: ")(
            static_cast<OTZMQWorkType>(value))
            .Flush();

        OT_FAIL;
    }
}

auto BlockDownloader::batch_ready() const noexcept -> void
{
    node_.JobReady(PeerManagerJobs::JobAvailableBlock);
}

auto BlockDownloader::batch_size(const std::size_t in) const noexcept
    -> std::size_t
{
    const auto limit = params::Chains().at(chain_).block_download_batch_;

    if (in < 10) {

        return std::min<std::size_t>(1, limit);
    } else if (in < 100) {

        return std::min<std::size_t>(10, limit);
    } else if (in < 1000) {

        return std::min<std::size_t>(100, limit);
    } else {

        return limit;
    }
}

auto BlockDownloader::check_task(TaskType& task) const noexcept -> void
{
    const auto& hash = task.position_.hash_;

    if (auto block = db_.BlockLoadBitcoin(hash); bool(block)) {
        tdiag("check_task about to download");
        task.download(std::move(block));
    }
}

auto BlockDownloader::pipeline(network::zeromq::Message&& in) -> void
{
    if (false == running_.load()) { return; }

    const auto body = in.Body();

    OT_ASSERT(1 <= body.size());
    const auto work = body.at(0).as<Work>();
    last_job_ = work;

    switch (work) {
        case Work::shutdown: {
            protect_shutdown([this] { shut_down(); });
        } break;
        case Work::block: {
            tdiag("block");
            if (dm_enabled()) { process_position(in); }
            run_if_enabled();
        } break;
        case Work::reorg: {
            tdiag("reorg");
            if (dm_enabled()) { process_position(in); }

            run_if_enabled();
        } break;
        case Work::heartbeat: {
            tdiag("heartbeat");
            if (dm_enabled()) { process_position(); }

            run_if_enabled();
        } break;
        case Work::statemachine: {
            run_if_enabled();
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto BlockDownloader::state_machine() noexcept -> int
{
    tdiag("BlockDownloader::state_machine");
    return BlockDMBlock::state_machine() ? 20 : 400;
}

auto BlockDownloader::last_job_str() const noexcept -> std::string
{
    return to_string(last_job_);
}

auto BlockDownloader::process_position(
    const network::zeromq::Message& in) noexcept -> void
{
    {
        const auto body = in.Body();

        OT_ASSERT(body.size() >= 4);

        const auto chain = body.at(1).as<blockchain::Type>();

        if (chain_ != chain) { return; }
    }

    process_position();
}

auto BlockDownloader::process_position() noexcept -> void
{
    auto current = known();
    auto hashes = header_.BestChain(current, 2000);

    OT_ASSERT(0 < hashes.size());

    auto prior = Previous{std::nullopt};
    {
        auto& first = hashes.front();

        if (first != current) {
            auto promise = std::promise<int>{};
            promise.set_value(0);
            prior.emplace(std::move(first), promise.get_future());
        }
    }
    hashes.erase(hashes.begin());
    update_position(std::move(hashes), 0, std::move(prior));
}

auto BlockDownloader::queue_processing(DownloadedData&& data) noexcept -> void
{
    if (0 == data.size()) { return; }

    for (const auto& task : data) {
        const auto& pBlock = task->data_.get();

        OT_ASSERT(pBlock);

        const auto& block = *pBlock;

        tdiag("storing block, size: ", (sizeof *pBlock));

        // TODO implement batch db updates
        if (db_.BlockStore(block)) {
            task->process(0);
        } else {
            task->redownload();
            break;
        }
    }
}

auto BlockDownloader::Shutdown() noexcept -> std::shared_future<void>
{
    return signal_shutdown();
}

auto BlockDownloader::shut_down() noexcept -> void
{
    stop();
    close_pipeline();
    // TODO MT-34 investigate what other actions might be needed
}

auto BlockDownloader::trigger_state_machine() const noexcept -> void
{
    trigger();
}

auto BlockDownloader::update_tip(const Position& position, const int&)
    const noexcept -> void
{
    const auto saved = db_.SetBlockTip(position);

    OT_ASSERT(saved);

    LogDetail()(print(chain_))(" block chain updated to height ")(
        position.height_)
        .Flush();
    auto work = MakeWork(OT_ZMQ_NEW_FULL_BLOCK_SIGNAL);
    work.AddFrame(position.height_);
    work.AddFrame(position.hash_);
    socket_->Send(std::move(work));
}

BlockDownloader::~BlockDownloader()
{
    try {
        Shutdown().get();
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
        // TODO MT-34 improve
    }
}
}  // namespace opentxs::blockchain::node::blockoracle
