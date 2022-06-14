// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_forward_declare opentxs::blockchain::node::implementation::FilterOracle::BlockIndexer
// IWYU pragma: no_forward_declare opentxs::blockchain::node::implementation::FilterOracle::BlockIndexerData

#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <future>
#include <iosfwd>
#include <memory>
#include <utility>

#include "blockchain/DownloadManager.hpp"
#include "blockchain/DownloadTask.hpp"
#include "blockchain/node/filteroracle/FilterOracle.hpp"
#include "core/Worker.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/database/Cfilter.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Hash.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Header.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Time.hpp"
#include "util/JobCounter.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
class Session;
}  // namespace api

namespace blockchain
{
namespace bitcoin
{
namespace block
{
class Block;
}  // namespace block
}  // namespace bitcoin

namespace database
{
class Cfilter;
}  // namespace database

namespace node
{
namespace implementation
{
class FilterOracle;
}  // namespace implementation

namespace internal
{
class BlockOracle;
class Manager;
}  // namespace internal

class HeaderOracle;
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace zeromq
{
namespace socket
{
class Push;
}  // namespace socket

class Message;
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::implementation
{
using BlockDMFilter = download::Manager<
    FilterOracle::BlockIndexer,
    std::shared_ptr<const bitcoin::block::Block>,
    cfilter::Header,
    cfilter::Type>;
using BlockWorkerFilter = Worker<api::Session>;

class FilterOracle::BlockIndexer final : public BlockDMFilter,
                                         public BlockWorkerFilter
{
public:
    auto NextBatch() noexcept { return allocate_batch(type_); }

    BlockIndexer(
        const api::Session& api,
        database::Cfilter& db,
        const HeaderOracle& header,
        const internal::BlockOracle& block,
        const internal::Manager& node,
        FilterOracle& parent,
        const blockchain::Type chain,
        const cfilter::Type type,
        const UnallocatedCString& shutdown,
        const NotifyCallback& notify) noexcept;

    ~BlockIndexer() final;

    auto last_job_str() const noexcept -> std::string final;

protected:
    auto pipeline(zmq::Message&& in) -> void final;
    auto state_machine() noexcept -> int final;

private:
    auto shut_down() noexcept -> void;

private:
    friend BlockDMFilter;

    database::Cfilter& db_;
    const HeaderOracle& header_;
    const internal::BlockOracle& block_;
    const internal::Manager& node_;
    FilterOracle& parent_;
    const blockchain::Type chain_;
    const cfilter::Type type_;
    const NotifyCallback& notify_;
    JobCounter job_counter_;
    FilterOracle::Work last_job_;

    auto batch_ready() const noexcept -> void { trigger(); }
    auto batch_size(const std::size_t in) const noexcept -> std::size_t;
    auto calculate_cfheaders(
        UnallocatedVector<BlockIndexerData>& cache) const noexcept -> bool;
    auto check_task(TaskType&) const noexcept -> void {}
    auto trigger_state_machine() const noexcept -> void { trigger(); }
    auto update_tip(const Position& position, const cfilter::Header&)
        const noexcept -> void;

    auto download() noexcept -> void;
    auto process_position(const zmq::Message& in) noexcept -> void;
    auto process_position(const Position& pos) noexcept -> void;
    auto queue_processing(DownloadedData&& data) noexcept -> void;
    auto reset_to_genesis() noexcept -> void;
};

struct FilterOracle::BlockIndexerData {
    using Task = FilterOracle::BlockIndexer::BatchType::TaskType;

    const Task& incoming_data_;
    const cfilter::Type type_;
    cfilter::Hash filter_hash_;
    database::Cfilter::CFilterParams& filter_data_;
    database::Cfilter::CFHeaderParams& header_data_;
    Outstanding& job_counter_;

    BlockIndexerData(
        cfilter::Hash blank,
        const Task& data,
        const cfilter::Type type,
        database::Cfilter::CFilterParams& filter,
        database::Cfilter::CFHeaderParams& header,
        Outstanding& jobCounter) noexcept
        : incoming_data_(data)
        , type_(type)
        , filter_hash_(std::move(blank))
        , filter_data_(filter)
        , header_data_(header)
        , job_counter_(jobCounter)
    {
    }
};
}  // namespace opentxs::blockchain::node::implementation
