// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <BlockchainTransactionInput.pb.h>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <optional>

#include "internal/blockchain/bitcoin/block/Input.hpp"
#include "internal/blockchain/bitcoin/block/Inputs.hpp"
#include "internal/blockchain/block/Block.hpp"
#include "internal/blockchain/block/Types.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Input.hpp"
#include "opentxs/blockchain/bitcoin/block/Inputs.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/Amount.hpp"
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
class Nym;
}  // namespace identifier

namespace proto
{
class BlockchainTransaction;
class BlockchainTransactionOutput;
}  // namespace proto

class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::bitcoin::block::implementation
{
class Inputs final : public internal::Inputs
{
public:
    using InputList = UnallocatedVector<std::unique_ptr<internal::Input>>;

    auto AssociatedLocalNyms(UnallocatedVector<identifier::Nym>& output)
        const noexcept -> void final;
    auto AssociatedRemoteContacts(
        UnallocatedVector<identifier::Generic>& output) const noexcept
        -> void final;
    auto at(const std::size_t position) const noexcept(false)
        -> const value_type& final
    {
        return *inputs_.at(position);
    }
    auto begin() const noexcept -> const_iterator final { return cbegin(); }
    auto CalculateSize(const bool normalized) const noexcept
        -> std::size_t final;
    auto cbegin() const noexcept -> const_iterator final { return {this, 0}; }
    auto cend() const noexcept -> const_iterator final
    {
        return {this, inputs_.size()};
    }
    auto clone() const noexcept -> std::unique_ptr<internal::Inputs> final
    {
        return std::make_unique<Inputs>(*this);
    }
    auto end() const noexcept -> const_iterator final { return cend(); }
    auto ExtractElements(const cfilter::Type style) const noexcept
        -> Vector<Vector<std::byte>> final;
    auto FindMatches(
        const blockchain::block::Txid& txid,
        const cfilter::Type type,
        const blockchain::block::Patterns& txos,
        const blockchain::block::ParsedPatterns& elements,
        const Log& log) const noexcept -> blockchain::block::Matches final;
    auto GetPatterns() const noexcept -> UnallocatedVector<PatternID> final;
    auto Internal() const noexcept -> const internal::Inputs& final
    {
        return *this;
    }
    auto Keys() const noexcept -> UnallocatedVector<crypto::Key> final;
    auto NetBalanceChange(const identifier::Nym& nym, const Log& log)
        const noexcept -> opentxs::Amount final;
    auto Serialize(const AllocateOutput destination) const noexcept
        -> std::optional<std::size_t> final;
    auto Serialize(proto::BlockchainTransaction& destination) const noexcept
        -> bool final;
    auto SerializeNormalized(const AllocateOutput destination) const noexcept
        -> std::optional<std::size_t> final;
    auto size() const noexcept -> std::size_t final { return inputs_.size(); }

    auto AnyoneCanPay(const std::size_t index) noexcept -> bool final;
    auto AssociatePreviousOutput(
        const std::size_t inputIndex,
        const internal::Output& output) noexcept -> bool final;
    auto Internal() noexcept -> internal::Inputs& final { return *this; }
    auto at(const std::size_t position) noexcept(false) -> value_type& final
    {
        return *inputs_.at(position);
    }
    auto MergeMetadata(const internal::Inputs& rhs, const Log& log) noexcept
        -> bool final;
    auto ReplaceScript(const std::size_t index) noexcept -> bool final;
    auto SetKeyData(const blockchain::block::KeyData& data) noexcept
        -> void final;

    Inputs(InputList&& inputs, std::optional<std::size_t> size = {}) noexcept(
        false);
    Inputs() = delete;
    Inputs(const Inputs&) noexcept;
    Inputs(Inputs&&) = delete;
    auto operator=(const Inputs&) -> Inputs& = delete;
    auto operator=(Inputs&&) -> Inputs& = delete;

    ~Inputs() final = default;

private:
    struct Cache {
        auto reset_size() noexcept -> void
        {
            auto lock = rLock{lock_};
            size_ = std::nullopt;
            normalized_size_ = std::nullopt;
        }
        template <typename F>
        auto size(const bool normalize, F cb) noexcept -> std::size_t
        {
            auto lock = rLock{lock_};

            auto& output = normalize ? normalized_size_ : size_;

            if (false == output.has_value()) { output = cb(); }

            return output.value();
        }

        Cache() noexcept = default;
        Cache(const Cache& rhs) noexcept
            : lock_()
            , size_()
            , normalized_size_()
        {
            auto lock = rLock{rhs.lock_};
            size_ = rhs.size_;
            normalized_size_ = rhs.normalized_size_;
        }

    private:
        mutable std::recursive_mutex lock_{};
        std::optional<std::size_t> size_{};
        std::optional<std::size_t> normalized_size_{};
    };

    const InputList inputs_;
    mutable Cache cache_;

    static auto clone(const InputList& rhs) noexcept -> InputList;

    auto serialize(const AllocateOutput destination, const bool normalize)
        const noexcept -> std::optional<std::size_t>;
};
}  // namespace opentxs::blockchain::bitcoin::block::implementation
