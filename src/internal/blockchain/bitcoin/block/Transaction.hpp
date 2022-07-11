// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <optional>

#include "internal/blockchain/bitcoin/Bitcoin.hpp"
#include "internal/blockchain/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"

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
namespace internal
{
class Output;
}  // namespace internal
}  // namespace block
}  // namespace bitcoin
}  // namespace blockchain

namespace identifier
{
class Generic;
}  // namespace identifier

namespace proto
{
class BlockchainTransaction;
}  // namespace proto

class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::bitcoin::block::internal
{
class Transaction : virtual public block::Transaction
{
public:
    using SerializeType = proto::BlockchainTransaction;
    using SigHash = blockchain::bitcoin::SigOption;

    virtual auto ConfirmationHeight() const noexcept
        -> blockchain::block::Height = 0;
    virtual auto GetPreimageBTC(
        const std::size_t index,
        const blockchain::bitcoin::SigHash& hashType) const noexcept
        -> Space = 0;
    // WARNING do not call this function if another thread has a non-const
    // reference to this object
    virtual auto MinedPosition() const noexcept
        -> const blockchain::block::Position& = 0;

    virtual auto AssociatePreviousOutput(
        const std::size_t inputIndex,
        const Output& output) noexcept -> bool = 0;
    virtual auto CalculateSize() const noexcept -> std::size_t = 0;
    virtual auto ExtractElements(const cfilter::Type style) const noexcept
        -> Vector<Vector<std::byte>> = 0;
    virtual auto FindMatches(
        const cfilter::Type type,
        const blockchain::block::Patterns& txos,
        const blockchain::block::ParsedPatterns& elements,
        const Log& log) const noexcept -> blockchain::block::Matches = 0;
    virtual auto GetPatterns() const noexcept
        -> UnallocatedVector<PatternID> = 0;
    virtual auto ForTestingOnlyAddKey(
        const std::size_t index,
        const blockchain::crypto::Key& key) noexcept -> bool = 0;
    virtual auto IDNormalized() const noexcept
        -> const identifier::Generic& = 0;
    virtual auto MergeMetadata(
        const blockchain::Type chain,
        const Transaction& rhs,
        const Log& log) noexcept -> void = 0;
    virtual auto Serialize(const AllocateOutput destination) const noexcept
        -> std::optional<std::size_t> = 0;
    virtual auto Serialize() const noexcept -> std::optional<SerializeType> = 0;
    virtual auto SetKeyData(const blockchain::block::KeyData& data) noexcept
        -> void = 0;
    virtual auto SetMemo(const UnallocatedCString& memo) noexcept -> void = 0;
    virtual auto SetMinedPosition(
        const blockchain::block::Position& pos) noexcept -> void = 0;
    virtual auto SetPosition(std::size_t position) noexcept -> void = 0;

    ~Transaction() override = default;
};
}  // namespace opentxs::blockchain::bitcoin::block::internal
