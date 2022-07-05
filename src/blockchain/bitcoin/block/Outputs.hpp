// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <BlockchainTransactionOutput.pb.h>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <optional>

#include "internal/blockchain/bitcoin/block/Output.hpp"
#include "internal/blockchain/bitcoin/block/Outputs.hpp"
#include "internal/blockchain/block/Block.hpp"
#include "internal/blockchain/block/Types.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Output.hpp"
#include "opentxs/blockchain/bitcoin/block/Outputs.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace proto
{
class BlockchainTransaction;
}  // namespace proto

class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::bitcoin::block::implementation
{
class Outputs final : public internal::Outputs
{
public:
    using OutputList = UnallocatedVector<std::unique_ptr<internal::Output>>;

    auto AssociatedLocalNyms(UnallocatedVector<OTNymID>& output) const noexcept
        -> void final;
    auto AssociatedRemoteContacts(
        UnallocatedVector<OTIdentifier>& output) const noexcept -> void final;
    auto at(const std::size_t position) const noexcept(false)
        -> const value_type& final
    {
        return *outputs_.at(position);
    }
    auto begin() const noexcept -> const_iterator final { return cbegin(); }
    auto CalculateSize() const noexcept -> std::size_t final;
    auto cbegin() const noexcept -> const_iterator final { return {this, 0}; }
    auto cend() const noexcept -> const_iterator final
    {
        return {this, outputs_.size()};
    }
    auto clone() const noexcept -> std::unique_ptr<internal::Outputs> final
    {
        return std::make_unique<Outputs>(*this);
    }
    auto end() const noexcept -> const_iterator final { return cend(); }
    auto ExtractElements(const cfilter::Type style) const noexcept
        -> Vector<Vector<std::byte>> final;
    auto FindMatches(
        const blockchain::block::Txid& txid,
        const cfilter::Type type,
        const blockchain::block::ParsedPatterns& elements,
        const Log& log) const noexcept -> blockchain::block::Matches final;
    auto GetPatterns() const noexcept -> UnallocatedVector<PatternID> final;
    auto Keys() const noexcept -> UnallocatedVector<crypto::Key> final;
    auto Internal() const noexcept -> const internal::Outputs& final
    {
        return *this;
    }
    auto NetBalanceChange(const identifier::Nym& nym, const Log& log)
        const noexcept -> opentxs::Amount final;
    auto Serialize(const AllocateOutput destination) const noexcept
        -> std::optional<std::size_t> final;
    auto Serialize(proto::BlockchainTransaction& destination) const noexcept
        -> bool final;
    auto SetKeyData(const blockchain::block::KeyData& data) noexcept
        -> void final;
    auto size() const noexcept -> std::size_t final { return outputs_.size(); }

    auto at(const std::size_t position) noexcept(false) -> value_type& final
    {
        return *outputs_.at(position);
    }
    auto ForTestingOnlyAddKey(
        const std::size_t index,
        const blockchain::crypto::Key& key) noexcept -> bool final;
    auto Internal() noexcept -> internal::Outputs& final { return *this; }
    auto MergeMetadata(const internal::Outputs& rhs, const Log& log) noexcept
        -> bool final;

    Outputs(
        OutputList&& outputs,
        std::optional<std::size_t> size = {}) noexcept(false);
    Outputs() = delete;
    Outputs(const Outputs&) noexcept;
    Outputs(Outputs&&) = delete;
    auto operator=(const Outputs&) -> Outputs& = delete;
    auto operator=(Outputs&&) -> Outputs& = delete;

    ~Outputs() final = default;

private:
    struct Cache {
        auto reset_size() noexcept -> void
        {
            auto lock = Lock{lock_};
            size_ = std::nullopt;
        }
        template <typename F>
        auto size(F cb) noexcept -> std::size_t
        {
            auto lock = Lock{lock_};

            auto& output = size_;

            if (false == output.has_value()) { output = cb(); }

            return output.value();
        }

        Cache() noexcept = default;
        Cache(const Cache& rhs) noexcept
            : lock_()
            , size_()
        {
            auto lock = Lock{rhs.lock_};
            size_ = rhs.size_;
        }

    private:
        mutable std::mutex lock_{};
        std::optional<std::size_t> size_{};
    };

    const OutputList outputs_;
    mutable Cache cache_;

    static auto clone(const OutputList& rhs) noexcept -> OutputList;
};
}  // namespace opentxs::blockchain::bitcoin::block::implementation
