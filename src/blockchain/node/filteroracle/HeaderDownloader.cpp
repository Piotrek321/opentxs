// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "blockchain/node/filteroracle/HeaderDownloader.hpp"  // IWYU pragma: associated

#include "blockchain/node/filteroracle/FilterDownloader.hpp"
#include "internal/blockchain/database/Cfilter.hpp"
#include "internal/blockchain/node/Manager.hpp"

namespace opentxs::blockchain::node::implementation
{
auto FilterOracle::HeaderDownloader::NextBatch() noexcept -> BatchType
{
    return allocate_batch(type_);
}

FilterOracle::HeaderDownloader::HeaderDownloader(
    const api::Session& api,
    database::Cfilter& db,
    const HeaderOracle& header,
    const internal::Manager& node,
    FilterOracle::FilterDownloader& filter,
    const blockchain::Type chain,
    const cfilter::Type type,
    const UnallocatedCString& shutdown,
    Callback&& cb) noexcept
    : HeaderDM(
          [&] { return db.FilterHeaderTip(type); }(),
          [&] {
              auto promise = std::promise<cfilter::Header>{};
              const auto tip = db.FilterHeaderTip(type);
              promise.set_value(db.LoadFilterHeader(type, tip.hash_.Bytes()));

              return Finished{promise.get_future()};
          }(),
          "cfheader",
          20000,
          10000)
    , HeaderWorker(api, 20ms)
    , db_(db)
    , header_(header)
    , node_(node)
    , filter_(filter)
    , chain_(chain)
    , type_(type)
    , checkpoint_(std::move(cb))
{
    init_executor(
        {shutdown, UnallocatedCString{api_.Endpoints().BlockchainReorg()}});

    OT_ASSERT(checkpoint_);
}

FilterOracle::HeaderDownloader::~HeaderDownloader()
{
    try {
        signal_shutdown().get();
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
        // TODO MT-34 improve
    }
}

auto FilterOracle::HeaderDownloader::batch_ready() const noexcept -> void
{
    node_.JobReady(PeerManagerJobs::JobAvailableCfheaders);
}

auto FilterOracle::HeaderDownloader::batch_size(const std::size_t in) noexcept
    -> std::size_t
{
    if (in < 10) {

        return 1;
    } else if (in < 100) {

        return 10;
    } else if (in < 1000) {

        return 100;
    } else {

        return 2000;
    }
}

auto FilterOracle::HeaderDownloader::check_task(TaskType&) const noexcept
    -> void
{
}

auto FilterOracle::HeaderDownloader::trigger_state_machine() const noexcept
    -> void
{
    trigger();
}

auto FilterOracle::HeaderDownloader::update_tip(
    const Position& position,
    const cfilter::Header&) const noexcept -> void
{
    const auto saved = db_.SetFilterHeaderTip(type_, position);

    OT_ASSERT(saved);

    LogDetail()(print(chain_))(" cfheader chain updated to height ")(
        position.height_)
        .Flush();
    filter_.UpdatePosition(position);
}

auto FilterOracle::HeaderDownloader::process_position(
    const zmq::Message& in) noexcept -> void
{
    {
        const auto body = in.Body();

        OT_ASSERT(body.size() >= 4);

        const auto chain = body.at(1).as<blockchain::Type>();

        if (chain_ != chain) { return; }
    }

    process_position();
}

auto FilterOracle::HeaderDownloader::process_position() noexcept -> void
{
    auto current = known();
    auto hashes = header_.BestChain(current, 20000);

    OT_ASSERT(!hashes.empty());

    auto prior = Previous{std::nullopt};
    {
        auto& first = hashes.front();

        if (first != current) {
            auto promise = std::promise<cfilter::Header>{};
            promise.set_value(db_.LoadFilterHeader(type_, first.hash_.Bytes()));
            prior.emplace(std::move(first), promise.get_future());
        }
    }
    hashes.erase(hashes.begin());
    update_position(std::move(hashes), type_, std::move(prior));
}

auto FilterOracle::HeaderDownloader::process_reset(
    const zmq::Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(3 < body.size());

    auto position =
        Position{body.at(1).as<block::Height>(), body.at(2).Bytes()};
    auto promise = std::promise<cfilter::Header>{};
    promise.set_value(body.at(3).Bytes());
    Reset(position, promise.get_future());
}

auto FilterOracle::HeaderDownloader::queue_processing(
    DownloadedData&& data) noexcept -> void
{
    if (data.empty()) { return; }

    auto hashes = Vector<cfilter::Hash>{};
    auto headers = Vector<database::Cfilter::CFHeaderParams>{};

    for (const auto& task : data) {
        const auto& hash = hashes.emplace_back(task->data_.get());
        auto header = blockchain::internal::FilterHashToHeader(
            api_, hash.Bytes(), task->previous_.get().Bytes());
        const auto& position = task->position_;
        const auto check = checkpoint_(position, header);

        if (check == position) {
            headers.emplace_back(position.hash_, header, hash);
            task->process(std::move(header));
        } else {
            const auto good = db_.LoadFilterHeader(type_, check.hash_.Bytes());

            OT_ASSERT(false == good.IsNull());

            auto work = MakeWork(Work::reset_filter_tip);
            work.AddFrame(check.height_);
            work.AddFrame(check.hash_);
            work.AddFrame(good);
            pipeline_.Push(std::move(work));
        }
    }

    const auto saved = db_.StoreFilterHeaders(type_, std::move(headers));

    OT_ASSERT(saved);
}

auto FilterOracle::HeaderDownloader::pipeline(zmq::Message&& in) -> void
{
    if (!running_.load()) { return; }

    const auto body = in.Body();

    OT_ASSERT(1 <= body.size());

    const auto work = body.at(0).as<FilterOracle::Work>();

    switch (work) {
        case FilterOracle::Work::shutdown: {
            protect_shutdown([this] { shut_down(); });
        } break;
        case FilterOracle::Work::block:
        case FilterOracle::Work::reorg: {
            process_position(in);
            run_if_enabled();
        } break;
        case FilterOracle::Work::reset_filter_tip: {
            process_reset(in);
        } break;
        case FilterOracle::Work::heartbeat: {
            process_position();
            run_if_enabled();
        } break;
        case FilterOracle::Work::statemachine: {
            run_if_enabled();
        } break;
        default: {
            OT_FAIL;
        }
    }
}
auto FilterOracle::HeaderDownloader::state_machine() noexcept -> bool
{
    return HeaderDM::state_machine();
}
auto FilterOracle::HeaderDownloader::shut_down() noexcept -> void
{
    close_pipeline();
    // TODO MT-34 investigate what other actions might be needed
}

}  // namespace opentxs::blockchain::node::implementation
