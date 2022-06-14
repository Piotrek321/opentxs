// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/filteroracle/FilterOracle.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "blockchain/DownloadTask.hpp"
#include "blockchain/node/filteroracle/BlockIndexer.hpp"
#include "blockchain/node/filteroracle/FilterCheckpoints.hpp"
#include "blockchain/node/filteroracle/FilterDownloader.hpp"
#include "blockchain/node/filteroracle/HeaderDownloader.hpp"
#include "internal/api/network/Blockchain.hpp"
#include "internal/api/session/Endpoints.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/block/Block.hpp"
#include "internal/blockchain/database/Cfilter.hpp"
#include "internal/blockchain/node/Config.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/node/Types.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/GCS.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Hash.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Header.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/FixedByteArray.hpp"
#include "opentxs/network/p2p/Block.hpp"
#include "opentxs/network/p2p/Data.hpp"
#include "opentxs/network/p2p/Types.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/Types.hpp"
#include "util/ScopeGuard.hpp"

namespace opentxs::factory
{
auto BlockchainFilterOracle(
    const api::Session& api,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::HeaderOracle& header,
    const blockchain::node::internal::BlockOracle& block,
    blockchain::database::Cfilter& database,
    const blockchain::Type chain,
    const blockchain::cfilter::Type filter,
    const UnallocatedCString& shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::FilterOracle>
{
    using ReturnType = opentxs::blockchain::node::implementation::FilterOracle;

    return std::make_unique<ReturnType>(
        api, config, node, header, block, database, chain, filter, shutdown);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::implementation
{

auto FilterOracle::to_str(FilterOracle::Work value) -> std::string
{
    static auto Map = std::map<FilterOracle::Work, std::string>{
        {Work::shutdown, "shutdown"},
        {Work::block, "block"},
        {Work::reorg, "reorg"},
        {Work::reset_filter_tip, "reset_filter_tip"},
        {Work::heartbeat, "heartbeat"},
        {Work::full_block, "full_block"},
        {Work::statemachine, "statemachine"}};
    auto i = Map.find(value);
    return i == Map.end() ? std::string{"???"} : i->second;
}

FilterOracle::FilterOracle(
    const api::Session& api,
    const internal::Config& config,
    const internal::Manager& node,
    const HeaderOracle& header,
    const internal::BlockOracle& block,
    database::Cfilter& database,
    const blockchain::Type chain,
    const blockchain::cfilter::Type filter,
    const UnallocatedCString& shutdown) noexcept
    : internal::FilterOracle()
    , api_(api)
    , node_(node)
    , header_(header)
    , database_(database)
    , filter_notifier_(api_.Network().Blockchain().Internal().FilterUpdate())
    , chain_(chain)
    , default_type_(filter)
    , lock_()
    , new_filters_([&] {
        auto socket = api_.Network().ZeroMQ().PublishSocket();
        auto started = socket->Start(
            api_.Endpoints().Internal().BlockchainFilterUpdated(chain_).data());

        OT_ASSERT(started);

        return socket;
    }())
    , cb_([this](const auto type, const auto& pos) {
        if (!running_) return;

        auto lock = rLock{lock_};
        new_tip(lock, type, pos);
    })
    , filter_downloader_([&]() -> std::unique_ptr<FilterDownloader> {
        if (config.download_cfilters_) {
            return std::make_unique<FilterDownloader>(
                api,
                database_,
                header_,
                node_,
                chain,
                default_type_,
                shutdown,
                cb_);
        } else {
            return {};
        }
    }())
    , header_downloader_([&]() -> std::unique_ptr<HeaderDownloader> {
        if (config.download_cfilters_) {
            return std::make_unique<HeaderDownloader>(
                api,
                database_,
                header_,
                node_,
                *filter_downloader_,
                chain,
                default_type_,
                shutdown,
                [&](const auto& position, const auto& header) {
                    return compare_header_to_checkpoint(position, header);
                });
        } else {
            return {};
        }
    }())
    , block_indexer_([&]() -> std::unique_ptr<BlockIndexer> {
        if (config.generate_cfilters_) {
            return std::make_unique<BlockIndexer>(
                api,
                database_,
                header_,
                block,
                node_,
                *this,
                chain,
                default_type_,
                shutdown,
                cb_);
        } else {
            return {};
        }
    }())
    , last_sync_progress_()
    , last_broadcast_()
    , outstanding_jobs_()
    , running_(true)
{
    OT_ASSERT(cb_);

    compare_tips_to_header_chain();
    compare_tips_to_checkpoint();
}

auto FilterOracle::compare_header_to_checkpoint(
    const block::Position& block,
    const cfilter::Header& receivedHeader) noexcept -> block::Position
{
    const auto& cp = filter_checkpoints_.at(chain_);
    const auto height = block.first;

    if (auto it = cp.find(height); cp.end() != it) {
        const auto& bytes = it->second.at(default_type_);
        const auto expectedHeader =
            api_.Factory().DataFromBytes(ReadView{bytes.data(), bytes.size()});

        if (expectedHeader == receivedHeader) {
            LogConsole()(print(chain_))(" filter header at height ")(
                height)(" verified against checkpoint")
                .Flush();

            return block;
        } else {
            OT_ASSERT(cp.begin() != it);

            std::advance(it, -1);
            const auto rollback =
                block::Position{it->first, header_.BestHash(it->first)};
            LogConsole()(print(chain_))(" filter header at height ")(
                height)(" does not match checkpoint. Resetting to previous "
                        "checkpoint at height ")(rollback.first)
                .Flush();

            return rollback;
        }
    }

    return block;
}

auto FilterOracle::compare_tips_to_checkpoint() noexcept -> void
{
    const auto& cp = filter_checkpoints_.at(chain_);
    const auto headerTip = database_.FilterHeaderTip(default_type_);
    auto checkPosition{headerTip};
    auto changed{false};

    for (auto i{cp.crbegin()}; i != cp.crend(); ++i) {
        const auto& cpHeight = i->first;

        if (cpHeight > checkPosition.first) { continue; }

        checkPosition = block::Position{cpHeight, header_.BestHash(cpHeight)};
        const auto existingHeader = database_.LoadFilterHeader(
            default_type_, checkPosition.second.Bytes());

        try {
            const auto& cpHeader = i->second.at(default_type_);
            const auto cpBytes = api_.Factory().DataFromHex(cpHeader);

            if (existingHeader == cpBytes) { break; }

            changed = true;
        } catch (...) {
            break;
        }
    }

    if (changed) {
        LogConsole()(print(chain_))(
            " filter header chain did not match checkpoint. Resetting to last "
            "known good position")
            .Flush();
        reset_tips_to(default_type_, headerTip, checkPosition, changed);
    } else {
        LogVerbose()(print(chain_))(" filter header chain matched checkpoint")
            .Flush();
    }
}

auto FilterOracle::compare_tips_to_header_chain() noexcept -> bool
{
    const auto current = database_.FilterHeaderTip(default_type_);
    const auto [parent, best] = header_.CommonParent(current);

    if ((parent.first == current.first) && (parent.second == current.second)) {
        LogVerbose()(print(chain_))(
            " filter header chain is following the best chain")
            .Flush();

        return false;
    }

    LogConsole()(print(chain_))(
        " filter header chain is following a sibling chain. Resetting to "
        "common ancestor at height ")(parent.first)
        .Flush();
    reset_tips_to(default_type_, current, parent);

    return true;
}

auto FilterOracle::FilterTip(const cfilter::Type type) const noexcept
    -> block::Position
{
    return database_.FilterTip(type);
}

auto FilterOracle::GetFilterJob() const noexcept -> CfilterJob
{
    auto lock = rLock{lock_};

    if (filter_downloader_) {

        return filter_downloader_->NextBatch();
    } else {

        return {};
    }
}

auto FilterOracle::GetHeaderJob() const noexcept -> CfheaderJob
{
    auto lock = rLock{lock_};

    if (header_downloader_) {

        return header_downloader_->NextBatch();
    } else {

        return {};
    }
}

auto FilterOracle::Heartbeat() const noexcept -> void
{
    auto lock = rLock{lock_};

    if (filter_downloader_) { filter_downloader_->Heartbeat(); }
    if (header_downloader_) { header_downloader_->Heartbeat(); }
    if (block_indexer_) { block_indexer_->Heartbeat(); }

    constexpr auto limit = 5s;

    if ((Clock::now() - last_sync_progress_) > limit) {
        new_tip(lock, default_type_, database_.FilterTip(default_type_));
    }
}

auto FilterOracle::LoadFilter(
    const cfilter::Type type,
    const block::Hash& block,
    alloc::Default alloc) const noexcept -> GCS
{
    return database_.LoadFilter(type, block.Bytes(), alloc);
}

auto FilterOracle::LoadFilters(
    const cfilter::Type type,
    const Vector<block::Hash>& blocks) const noexcept -> Vector<GCS>
{
    return database_.LoadFilters(type, blocks);
}

auto FilterOracle::LoadFilterHeader(
    const cfilter::Type type,
    const block::Hash& block) const noexcept -> cfilter::Header
{
    return database_.LoadFilterHeader(type, block.Bytes());
}

auto FilterOracle::LoadFilterOrResetTip(
    const cfilter::Type type,
    const block::Position& position,
    alloc::Default alloc) const noexcept -> GCS
{
    auto output = LoadFilter(type, position.second, alloc);

    if (output.IsValid()) { return output; }

    const auto height = position.first;

    OT_ASSERT(0 < height);

    const auto parent = height - 1;
    const auto hash = header_.BestHash(parent);

    OT_ASSERT(false == hash.IsNull());

    reset_tips_to(type, block::Position{parent, hash}, false, true);

    return {};
}

auto FilterOracle::new_tip(
    const rLock&,
    const cfilter::Type type,
    const block::Position& tip) const noexcept -> void
{
    auto last = last_broadcast_.find(type);

    if (last_broadcast_.end() != last) {
        if (tip == last->second) {
            // NOTE: new tip is same as previously broadcast

            return;
        } else {
            // NOTE: new tip is different from previously broadcast
            last->second = tip;
        }
    } else {
        // NOTE: first tip broadcast
        last_broadcast_.emplace(type, tip);
    }

    last_sync_progress_ = Clock::now();

    {
        auto work = MakeWork(OT_ZMQ_NEW_FILTER_SIGNAL);
        work.AddFrame(type);
        work.AddFrame(tip.first);
        work.AddFrame(tip.second);
        new_filters_->Send(std::move(work));
    }
    {
        auto work = MakeWork(WorkType::BlockchainNewFilter);
        work.AddFrame(chain_);
        work.AddFrame(type);
        work.AddFrame(tip.first);
        work.AddFrame(tip.second);
        filter_notifier_.Send(std::move(work));
    }
}

auto FilterOracle::ProcessBlock(
    const bitcoin::block::Block& block) const noexcept -> bool
{
    const auto& id = block.ID();
    const auto& header = block.Header();
    auto filters = Vector<database::Cfilter::CFilterParams>{};
    auto headers = Vector<database::Cfilter::CFHeaderParams>{};
    const auto& cfilter =
        filters.emplace_back(id, process_block(default_type_, block)).second;

    if (!cfilter.IsValid()) {
        LogError()(OT_PRETTY_CLASS())("Failed to calculate ")(print(chain_))(
            " cfilter")
            .Flush();

        return false;
    }

    const auto previousCfheader =
        LoadFilterHeader(default_type_, header.ParentHash());

    if (previousCfheader.IsNull()) {
        LogError()(OT_PRETTY_CLASS())("failed to load previous")(print(chain_))(
            " cfheader")
            .Flush();

        return false;
    }

    const auto filterHash = cfilter.Hash();
    const auto& cfheader = std::get<1>(headers.emplace_back(
        id, cfilter.Header(previousCfheader.Bytes()), filterHash.Bytes()));

    if (cfheader.IsNull()) {
        LogError()(OT_PRETTY_CLASS())("failed to calculate ")(print(chain_))(
            " cfheader")
            .Flush();

        return false;
    }

    const auto position = make_blank<block::Position>::value(api_);
    const auto stored =
        database_.StoreFilters(default_type_, headers, filters, position);

    if (stored) {

        return true;
    } else {
        LogError()(OT_PRETTY_CLASS())("Database error ").Flush();

        return false;
    }
}

auto FilterOracle::ProcessBlock(BlockIndexerData& data) const noexcept -> void
{
    auto post = ScopeGuard{[&] { --data.job_counter_; }};
    auto& task = data.incoming_data_;
    const auto& [height, block] = task.position_;

    try {
        LogTrace()(OT_PRETTY_CLASS())("Calculating cfilter for ")(
            print(chain_))(" block at height ")(height)
            .Flush();
        auto& [blockHashView, cfilter] = data.filter_data_;
        auto& [blockHash, filterHeader, filterHashView] = data.header_data_;
        blockHash = block;
        blockHashView = blockHash.Bytes();
        const auto pBlock = task.data_.get();

        if (!bool(pBlock)) {
            LogError()(OT_PRETTY_CLASS())("Failed to load ")(print(chain_))(
                " block #")(height)
                .Flush();

            throw std::runtime_error(
                UnallocatedCString{"failed to load block "} +
                blockHash.asHex());
        }

        cfilter = process_block(data.type_, *pBlock);

        if (!cfilter.IsValid()) {
            LogError()(OT_PRETTY_CLASS())("Failed to instantiate ")(
                print(chain_))(" cfilter #")(height)
                .Flush();

            throw std::runtime_error("Failed to instantiate gcs");
        }

        data.filter_hash_ = cfilter.Hash();
        LogTrace()(OT_PRETTY_CLASS())("Finished calculating cfilter for ")(
            print(chain_))(" block at height ")(height)
            .Flush();
        filterHashView = data.filter_hash_;
    } catch (...) {
        task.process(std::current_exception());
    }
}

auto FilterOracle::ProcessSyncData(
    const block::Hash& prior,
    const Vector<block::Hash>& hashes,
    const network::p2p::Data& data) const noexcept -> void
{
    auto filters = Vector<database::Cfilter::CFilterParams>{};
    auto headers = Vector<database::Cfilter::CFHeaderParams>{};
    const auto& blocks = data.Blocks();
    const auto incoming = blocks.front().Height();
    const auto finalFilter = data.LastPosition(api_);
    const auto filterType = blocks.front().FilterType();
    const auto current = database_.FilterTip(filterType);
    const auto params = blockchain::internal::GetFilterParams(filterType);

    if ((1 == incoming) && (1000 < current.first)) {
        const auto height = current.first - 1000;
        reset_tips_to(
            filterType, block::Position{height, header_.BestHash(height)});

        return;
    }

    LogVerbose()(OT_PRETTY_CLASS())("current ")(print(chain_))(
        " filter tip height is ")(current.first)
        .Flush();
    LogVerbose()(OT_PRETTY_CLASS())("incoming ")(print(chain_))(
        " sync data provides heights ")(incoming)(" to ")(finalFilter.first)
        .Flush();

    if (incoming > (current.first + 1)) {
        LogVerbose()(OT_PRETTY_CLASS())("cannot connect ")(print(chain_))(
            " sync data to current tip")
            .Flush();

        return;
    }

    const auto redundant = (finalFilter.first < current.first) ||
                           (finalFilter.second == current.second);

    if (redundant) {
        LogVerbose()(OT_PRETTY_CLASS())("ignoring redundant ")(print(chain_))(
            " sync data")
            .Flush();

        return;
    }

    const auto count{std::min<std::size_t>(hashes.size(), blocks.size())};
    filters.reserve(count);
    headers.reserve(count);

    try {
        OT_ASSERT(0 < count);
        cfilter::Header previous{};
        if (!prior.empty()) {
            previous = LoadFilterHeader(filterType, prior);
            if (previous.IsNull()) {
                LogError()(OT_PRETTY_CLASS())("cfheader for ")(print(chain_))(
                    " block ")(prior.asHex())(" not found")
                    .Flush();

                throw std::runtime_error("Failed to load previous cfheader");
            }
        }

        auto* parent = &previous;
        auto b = hashes.cbegin();
        auto d = blocks.cbegin();

        for (auto i = std::size_t{0u}; i < count; ++i, ++b, ++d) {
            const auto& blockHash = *b;
            const auto& syncData = *d;
            const auto height = syncData.Height();
            auto& [fBlockHash, cfilter] = filters.emplace_back(
                blockHash,
                factory::GCS(
                    api_,
                    params.first,
                    params.second,
                    blockchain::internal::BlockHashToFilterKey(
                        blockHash.Bytes()),
                    syncData.FilterElements(),
                    syncData.Filter(),
                    {}));  // TODO allocator

            if (!cfilter.IsValid()) {
                LogError()(OT_PRETTY_CLASS())("Failed to instantiate ")(
                    print(chain_))(" cfilter #")(height)
                    .Flush();

                throw std::runtime_error("Failed to instantiate gcs");
            }

            auto& [hBlockHash, cfheader, cfhash] = headers.emplace_back(
                blockHash, cfilter.Header(*parent), cfilter.Hash());
            parent = &cfheader;
        }

        const auto last = count - 1u;
        const auto tip =
            block::Position{blocks.at(last).Height(), hashes.at(last)};

        const auto stored =
            database_.StoreFilters(filterType, headers, filters, tip);

        if (stored) {
            LogDetail()(print(chain_))(
                " cfheader and cfilter chain updated to height ")(tip.first)
                .Flush();
            cb_(filterType, tip);
        } else {
            throw std::runtime_error{"database error"};
        }
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
    }
}

auto FilterOracle::process_block(
    const cfilter::Type filterType,
    const bitcoin::block::Block& block) const noexcept -> GCS
{
    const auto& id = block.ID();
    const auto params = blockchain::internal::GetFilterParams(filterType);

    const auto input = block.Internal().ExtractElements(filterType);
    Vector<OTData> elements{};
    std::transform(
        input.begin(),
        input.end(),
        std::back_inserter(elements),
        [&](const auto& element) -> OTData {
            return api_.Factory().DataFromBytes(reader(element));
        });

    return factory::GCS(
        api_,
        params.first,
        params.second,
        blockchain::internal::BlockHashToFilterKey(id.Bytes()),
        elements,
        {});  // TODO allocator
}

auto FilterOracle::reset_tips_to(
    const cfilter::Type type,
    const block::Position& position,
    const std::optional<bool> resetHeader,
    const std::optional<bool> resetFilter) const noexcept -> bool
{
    return reset_tips_to(
        type,
        database_.FilterHeaderTip(default_type_),
        database_.FilterTip(default_type_),
        position,
        resetHeader,
        resetFilter);
}

auto FilterOracle::reset_tips_to(
    const cfilter::Type type,
    const block::Position& headerTip,
    const block::Position& position,
    const std::optional<bool> resetHeader) const noexcept -> bool
{
    return reset_tips_to(
        type,
        headerTip,
        database_.FilterTip(default_type_),
        position,
        resetHeader);
}

auto FilterOracle::reset_tips_to(
    const cfilter::Type type,
    const block::Position& headerTip,
    const block::Position& filterTip,
    const block::Position& position,
    std::optional<bool> resetHeader,
    std::optional<bool> resetFilter) const noexcept -> bool
{
    auto counter{0};

    if (!resetHeader.has_value()) { resetHeader = headerTip > position; }

    if (!resetFilter.has_value()) { resetFilter = filterTip > position; }

    OT_ASSERT(resetHeader.has_value());
    OT_ASSERT(resetFilter.has_value());

    auto lock = rLock{lock_};
    using Future = std::shared_future<cfilter::Header>;

    const auto& block = header_.LoadHeader(position.second);

    OT_ASSERT(block);

    auto promise = std::promise<cfilter::Header>{};
    promise.set_value(
        database_.LoadFilterHeader(default_type_, block->ParentHash().Bytes()));

    std::shared_future<cfilter::Header> previous = promise.get_future();

    auto resetBlock{false};
    auto headerTipHasBeenReset{false};
    auto filterTipHasBeenReset{false};

    if (resetHeader.value()) {
        if (header_downloader_) {
            header_downloader_->Reset(position, Future{previous});
            headerTipHasBeenReset = true;
        }

        resetBlock = true;
        ++counter;
    }

    if (resetFilter.value()) {
        if (filter_downloader_) {
            filter_downloader_->Reset(position, Future{previous});
            filterTipHasBeenReset = true;
        }

        resetBlock = true;
        ++counter;
    }

    if (resetBlock && block_indexer_) {
        block_indexer_->Reset(position, std::move(previous));
        headerTipHasBeenReset = true;
        filterTipHasBeenReset = true;
    }

    if (resetHeader.value() && !headerTipHasBeenReset) {
        database_.SetFilterHeaderTip(default_type_, position);
    }

    if (resetFilter.value() && !filterTipHasBeenReset) {
        database_.SetFilterTip(default_type_, position);
    }

    return 0 < counter;
}

auto FilterOracle::Shutdown() noexcept -> void
{
    running_ = false;

    auto lock = rLock{lock_};

    if (header_downloader_) { header_downloader_.reset(); }

    if (filter_downloader_) { filter_downloader_.reset(); }

    if (block_indexer_) { block_indexer_.reset(); }
}

auto FilterOracle::Start() noexcept -> void
{
    auto lock = rLock{lock_};

    if (header_downloader_) { header_downloader_->Start(); }

    if (filter_downloader_) { filter_downloader_->Start(); }

    if (block_indexer_) { block_indexer_->Start(); }
}

auto FilterOracle::Tip(const cfilter::Type type) const noexcept
    -> block::Position
{
    return database_.FilterTip(type);
}

FilterOracle::~FilterOracle() { Shutdown(); }
}  // namespace opentxs::blockchain::node::implementation
