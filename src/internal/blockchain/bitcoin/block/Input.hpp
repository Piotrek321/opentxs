// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>

#include "internal/blockchain/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/block/Input.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
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
class Script;
}  // namespace internal
}  // namespace block
}  // namespace bitcoin
}  // namespace blockchain

namespace identifier
{
class Nym;
}  // namespace identifier

class Amount;
class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::bitcoin::block::internal
{
class Input : virtual public block::Input
{
public:
    using SerializeType = proto::BlockchainTransactionInput;
    using Signature = std::pair<ReadView, ReadView>;
    using Signatures = UnallocatedVector<Signature>;

    virtual auto AssociatedLocalNyms(
        UnallocatedVector<identifier::Nym>& output) const noexcept -> void = 0;
    virtual auto AssociatedRemoteContacts(
        UnallocatedVector<identifier::Generic>& output) const noexcept
        -> void = 0;
    virtual auto CalculateSize(const bool normalized = false) const noexcept
        -> std::size_t = 0;
    virtual auto clone() const noexcept -> std::unique_ptr<Input> = 0;
    virtual auto ExtractElements(const cfilter::Type style) const noexcept
        -> Vector<Vector<std::byte>> = 0;
    virtual auto FindMatches(
        const blockchain::block::Txid& txid,
        const cfilter::Type type,
        const blockchain::block::Patterns& txos,
        const blockchain::block::ParsedPatterns& elements,
        const std::size_t position,
        const Log& log) const noexcept -> blockchain::block::Matches = 0;
    virtual auto GetBytes(std::size_t& base, std::size_t& witness)
        const noexcept -> void = 0;
    virtual auto GetPatterns() const noexcept
        -> UnallocatedVector<PatternID> = 0;
    virtual auto NetBalanceChange(
        const identifier::Nym& nym,
        const std::size_t index,
        const Log& log) const noexcept -> opentxs::Amount = 0;
    virtual auto Serialize(
        const AllocateOutput destination) const noexcept  // TODO: MT-85
        -> std::optional<std::size_t> = 0;
    virtual auto Serialize(
        const std::uint32_t index,
        SerializeType& destination) const noexcept -> bool = 0;
    virtual auto SerializeNormalized(const AllocateOutput destination)
        const noexcept -> std::optional<std::size_t> = 0;
    virtual auto SignatureVersion() const noexcept
        -> std::unique_ptr<Input> = 0;
    virtual auto SignatureVersion(std::unique_ptr<internal::Script> subscript)
        const noexcept -> std::unique_ptr<Input> = 0;
    virtual auto Spends() const noexcept(false) -> const Output& = 0;

    virtual auto AddMultisigSignatures(const Signatures& signatures) noexcept
        -> bool = 0;
    virtual auto AddSignatures(const Signatures& signatures) noexcept
        -> bool = 0;
    virtual auto AssociatePreviousOutput(const Output& output) noexcept
        -> bool = 0;
    virtual auto MergeMetadata(
        const Input& rhs,
        const std::size_t index,
        const Log& log) noexcept -> bool = 0;
    virtual auto ReplaceScript() noexcept -> bool = 0;
    virtual auto SetKeyData(const blockchain::block::KeyData& data) noexcept
        -> void = 0;

    ~Input() override = default;
};
}  // namespace opentxs::blockchain::bitcoin::block::internal
