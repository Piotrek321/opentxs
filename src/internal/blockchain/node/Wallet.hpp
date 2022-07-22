// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <future>
#include <optional>

#include "opentxs/blockchain/node/Types.hpp"
#include "opentxs/blockchain/node/Wallet.hpp"
#include "util/Work.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace proto
{
class BlockchainTransactionProposal;
}  // namespace proto

class Amount;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::internal
{
class Wallet : virtual public node::Wallet
{
public:
    enum class Task : OTZMQWorkType {
        index = OT_ZMQ_INTERNAL_SIGNAL + 0,
        scan = OT_ZMQ_INTERNAL_SIGNAL + 1,
        process = OT_ZMQ_INTERNAL_SIGNAL + 2,
        reorg = OT_ZMQ_INTERNAL_SIGNAL + 3,
    };

    virtual auto ConstructTransaction(
        const proto::BlockchainTransactionProposal& tx,
        std::promise<SendOutcome>&& promise) const noexcept -> void = 0;
    virtual auto FeeEstimate() const noexcept -> std::optional<Amount> = 0;
    auto Internal() const noexcept -> const internal::Wallet& final
    {
        return *this;
    }

    virtual auto Init() noexcept -> void = 0;
    virtual auto Shutdown() noexcept -> void = 0;
    auto Internal() noexcept -> internal::Wallet& final { return *this; }

    ~Wallet() override = default;
};
}  // namespace opentxs::blockchain::node::internal
