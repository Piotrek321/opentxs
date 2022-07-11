// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "internal/blockchain/block/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Outputs.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace identifier
{
class Nym;
}  // namespace identifier

namespace proto
{
class BlockchainTransaction;
}  // namespace proto
// }  // namespace v1

class Amount;
class Log;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::bitcoin::block::internal
{
class Outputs : virtual public block::Outputs
{
public:
    virtual auto AssociatedLocalNyms(
        UnallocatedVector<identifier::Nym>& output) const noexcept -> void = 0;
    virtual auto AssociatedRemoteContacts(
        UnallocatedVector<identifier::Generic>& output) const noexcept
        -> void = 0;
    virtual auto CalculateSize() const noexcept -> std::size_t = 0;
    virtual auto clone() const noexcept -> std::unique_ptr<Outputs> = 0;
    virtual auto ExtractElements(const cfilter::Type style) const noexcept
        -> Vector<Vector<std::byte>> = 0;
    virtual auto FindMatches(
        const blockchain::block::Txid& txid,
        const cfilter::Type type,
        const blockchain::block::ParsedPatterns& elements,
        const Log& log) const noexcept -> blockchain::block::Matches = 0;
    virtual auto GetPatterns() const noexcept
        -> UnallocatedVector<PatternID> = 0;
    virtual auto NetBalanceChange(const identifier::Nym& nym, const Log& log)
        const noexcept -> opentxs::Amount = 0;
    virtual auto Serialize(const AllocateOutput destination) const noexcept
        -> std::optional<std::size_t> = 0;
    virtual auto Serialize(
        proto::BlockchainTransaction& destination) const noexcept -> bool = 0;

    using block::Outputs::at;
    virtual auto at(const std::size_t position) noexcept(false)
        -> value_type& = 0;
    virtual auto ForTestingOnlyAddKey(
        const std::size_t index,
        const blockchain::crypto::Key& key) noexcept -> bool = 0;
    virtual auto MergeMetadata(const Outputs& rhs, const Log& log) noexcept
        -> bool = 0;
    virtual auto SetKeyData(const blockchain::block::KeyData& data) noexcept
        -> void = 0;

    ~Outputs() override = default;
};
}  // namespace opentxs::blockchain::bitcoin::block::internal
