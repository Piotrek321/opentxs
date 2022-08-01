// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <iosfwd>
#include <memory>
#include <stdexcept>

#include "blockchain/bitcoin/p2p/Message.hpp"
#include "internal/blockchain/p2p/bitcoin/Bitcoin.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/p2p/Types.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Pimpl.hpp"

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
namespace p2p
{
namespace bitcoin
{
class Header;
}  // namespace bitcoin
}  // namespace p2p
}  // namespace blockchain
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::p2p::bitcoin::message
{
class Getblocks final : public implementation::Message
{
public:
    struct Raw {
        ProtocolVersionField version_;
        UnallocatedVector<BlockHeaderHashField> header_hashes_;
        BlockHeaderHashField stop_hash_;

        Raw(ProtocolVersionUnsigned version,
            const UnallocatedVector<ByteArray>& header_hashes,
            const Data& stop_hash) noexcept(false)
            : version_(version)
            , header_hashes_()
            , stop_hash_()
        {
            if (stop_hash.size() != sizeof(stop_hash_)) {
                throw std::runtime_error("Invalid stop hash");
            }

            std::memcpy(stop_hash_.data(), stop_hash.data(), stop_hash.size());

            for (const auto& hash : header_hashes) {
                BlockHeaderHashField tempHash;

                if (hash.size() != sizeof(tempHash)) {
                    throw std::runtime_error("Invalid hash");
                }

                std::memcpy(tempHash.data(), hash.data(), hash.size());
                header_hashes_.push_back(tempHash);
            }
        }
        Raw() noexcept
            : version_(2)  // TODO
            , header_hashes_()
            , stop_hash_()
        {
        }
    };

    auto getHashes() const noexcept -> const UnallocatedVector<ByteArray>&
    {
        return header_hashes_;
    }
    auto getStopHash() const noexcept -> ByteArray { return stop_hash_; }
    auto hashCount() const noexcept -> std::size_t
    {
        return header_hashes_.size();
    }
    using implementation::Message::payload;
    auto payload(AllocateOutput out) const noexcept -> bool final;
    auto version() const noexcept -> bitcoin::ProtocolVersionUnsigned
    {
        return version_;
    }

    Getblocks(
        const api::Session& api,
        const blockchain::Type network,
        const bitcoin::ProtocolVersionUnsigned version,
        const UnallocatedVector<ByteArray>& header_hashes,
        const Data& stop_hash) noexcept;
    Getblocks(
        const api::Session& api,
        std::unique_ptr<Header> header,
        const bitcoin::ProtocolVersionUnsigned version,
        const UnallocatedVector<ByteArray>& header_hashes,
        const Data& stop_hash) noexcept(false);
    Getblocks(const Getblocks&) = delete;
    Getblocks(Getblocks&&) = delete;
    auto operator=(const Getblocks&) -> Getblocks& = delete;
    auto operator=(Getblocks&&) -> Getblocks& = delete;

    ~Getblocks() final = default;

private:
    const bitcoin::ProtocolVersionUnsigned version_;
    const UnallocatedVector<ByteArray> header_hashes_;
    const ByteArray stop_hash_;
};
}  // namespace opentxs::blockchain::p2p::bitcoin::message
