// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "internal/blockchain/node/wallet/subchain/statemachine/Scan.hpp"

#include <boost/smart_ptr/shared_ptr.hpp>
#include <atomic>
#include <optional>

#include "blockchain/node/wallet/subchain/statemachine/Job.hpp"
#include "internal/blockchain/node/wallet/Types.hpp"
#include "internal/blockchain/node/wallet/subchain/statemachine/Types.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Container.hpp"
#include "util/Actor.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace blockchain
{
namespace block
{
class Position;
}  // namespace block

namespace node
{
namespace wallet
{
class SubchainStateData;
}  // namespace wallet
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace zeromq
{
namespace socket
{
class Raw;
}  // namespace socket
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::wallet
{
class Scan::Imp final : public statemachine::Job
{
public:
    auto ProcessReorg(
        const Lock& headerOracleLock,
        const block::Position& parent) noexcept -> void final;

    Imp(const boost::shared_ptr<const SubchainStateData>& parent,
        const network::zeromq::BatchID batch,
        allocator_type alloc) noexcept;
    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~Imp() final;

private:
    auto sProcessReorg(
        const Lock& headerOracleLock,
        const block::Position& parent) noexcept -> void;
    network::zeromq::socket::Raw& to_process_;
    std::optional<block::Position> last_scanned_;
    std::optional<block::Position> filter_tip_;
    bool enabled_;

private:
    auto caught_up() const noexcept -> bool;
    auto current() const noexcept -> const block::Position&;
    auto tip() const noexcept -> const block::Position&;

    auto do_startup() noexcept -> void final;
    auto process_do_rescan(Message&& in) noexcept -> void final;
    auto process_filter(Message&& in, block::Position&& tip) noexcept
        -> void final;
    auto work() noexcept -> int final;

    network::zeromq::Message make_work(Vector<ScanStatus>&& vec) const noexcept;
};
}  // namespace opentxs::blockchain::node::wallet
