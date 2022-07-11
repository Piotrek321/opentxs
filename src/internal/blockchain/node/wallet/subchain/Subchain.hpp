// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/smart_ptr/shared_ptr.hpp>
#include <atomic>
#include <memory>

#include "internal/blockchain/node/wallet/Types.hpp"
#include "internal/blockchain/node/wallet/subchain/statemachine/Types.hpp"
#include "util/LMDB.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace blockchain
{
namespace bitcoin
{
namespace block
{
class Transaction;
}  // namespace block
}  // namespace bitcoin

namespace node
{
namespace wallet
{
class SubchainStateData;
}  // namespace wallet
}  // namespace node
}  // namespace blockchain

namespace identifier
{
class Generic;
}  // namespace identifier
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::wallet
{
class Subchain
{
public:
    using State = JobState;

    [[nodiscard]] virtual auto ChangeState(
        const State state,
        StateSequence reorg) noexcept -> bool = 0;
    virtual auto Init(boost::shared_ptr<SubchainStateData> me) noexcept
        -> void = 0;
    virtual auto ProcessReorg(
        const Lock& headerOracleLock,
        storage::lmdb::LMDB::Transaction& tx,
        std::atomic_int& errors,
        const block::Position& ancestor) noexcept -> void = 0;

    Subchain(const Subchain&) = delete;
    Subchain(Subchain&&) = delete;
    auto operator=(const Subchain&) -> Subchain& = delete;
    auto operator=(Subchain&&) -> Subchain& = delete;

    virtual ~Subchain() = default;

protected:
    Subchain() = default;
};
}  // namespace opentxs::blockchain::node::wallet
