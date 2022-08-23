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

    ~Imp() final;

private:
    enum class State {
        normal,
        shutdown,
    };

    const api::Session& api_;
    const node::Manager& node_;
    const node::FilterOracle& parent_;
    database::Cfilter& db_;
    const blockchain::Type chain_;
    const cfilter::Type filter_type_;
    const NotifyCallback notify_;
    State state_;
    cfilter::Header previous_header_;
    cfilter::Header current_header_;
    block::Position best_position_;
    block::Position current_position_;

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
    auto work() noexcept -> int final;
    auto to_str(Work w) const noexcept -> std::string override;
};
}  // namespace opentxs::blockchain::node::filteroracle
