// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_forward_declare opentxs::blockchain::node::implementation::FilterOracle::BlockIndexer
// IWYU pragma: no_forward_declare opentxs::blockchain::node::implementation::FilterOracle::BlockIndexerData

#pragma once

#include <boost/smart_ptr/shared_ptr.hpp>
#include <string_view>

#include "internal/blockchain/node/filteroracle/BlockIndexer.hpp"
#include "internal/blockchain/node/filteroracle/Types.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Hash.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Header.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Container.hpp"
#include "util/Actor.hpp"

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
class Position;
}  // namespace block
}  // namespace bitcoin

namespace database
{
class Cfilter;
}  // namespace database

namespace node
{
class FilterOracle;
class Manager;
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace zeromq
{
class Message;
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::filteroracle
{
class BlockIndexer::Imp final : public Actor<BlockIndexerJob>
{
public:
    auto Init(boost::shared_ptr<Imp> me) noexcept -> void;
    auto Reindex() noexcept -> void;
    auto Shutdown() noexcept -> void;

    Imp(const api::Session& api,
        const node::Manager& node,
        const node::FilterOracle& parent,
        database::Cfilter& db,
        NotifyCallback&& notify,
        blockchain::Type chain,
        cfilter::Type type,
        std::string_view parentEndpoint,
        const network::zeromq::BatchID batch,
        allocator_type alloc) noexcept;
    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~BlockIndexer() final;

    auto last_job_str() const noexcept -> std::string final;

protected:
    auto pipeline(zmq::Message&& in) -> void final;
    auto state_machine() noexcept -> int final;

private:
    enum class State {
        normal,
        shutdown,
    };

    const api::Session& api_;
    const node::Manager& node_;
    const node::FilterOracle& parent_;
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

    auto calculate_next_block() noexcept -> bool;
    auto do_shutdown() noexcept -> void final;
    auto do_startup() noexcept -> void final;
    auto find_best_position(block::Position candidate) noexcept -> void;
    auto pipeline(const Work work, Message&& msg) noexcept -> void final;
    auto process_block(network::zeromq::Message&& in) noexcept -> void;
    auto process_block(block::Position&& position) noexcept -> void;
    auto process_reindex(network::zeromq::Message&& in) noexcept -> void;
    auto process_reorg(network::zeromq::Message&& in) noexcept -> void;
    auto process_reorg(block::Position&& commonParent) noexcept -> void;
    auto reset(block::Position&& to) noexcept -> void;
    auto state_normal(const Work work, network::zeromq::Message&& msg) noexcept
        -> void;
    auto transition_state_shutdown() noexcept -> void;
    auto update_position(
        const block::Position& previousCfheader,
        const block::Position& previousCfilter,
        const block::Position& newTip) noexcept -> void;
    auto work() noexcept -> bool final;
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
