// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/endian/buffers.hpp>
#include <boost/endian/conversion.hpp>
#include <array>
#include <cstdint>
#include <memory>

#include "blockchain/bitcoin/block/header/Header.hpp"
#include "blockchain/block/header/Header.hpp"
#include "blockchain/block/header/Imp.hpp"
#include "internal/blockchain/bitcoin/block/Header.hpp"
#include "internal/blockchain/block/Block.hpp"
#include "internal/blockchain/block/Header.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/NumericHash.hpp"
#include "opentxs/blockchain/bitcoin/Work.hpp"
#include "opentxs/blockchain/bitcoin/block/Header.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"
#include "opentxs/util/Time.hpp"

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
class Header;
}  // namespace block
}  // namespace bitcoin

namespace block
{
class Header;
}  // namespace block
}  // namespace blockchain

class ByteArray;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace be = boost::endian;

namespace opentxs::blockchain::bitcoin::block::implementation
{
class Header final : virtual public bitcoin::block::Header::Imp,
                     blockchain::block::implementation::Header
{
public:
    struct BitcoinFormat {
        be::little_int32_buf_t version_;
        std::array<char, 32> previous_;
        std::array<char, 32> merkle_;
        be::little_uint32_buf_t time_;
        be::little_uint32_buf_t nbits_;
        be::little_uint32_buf_t nonce_;

        auto Target() const noexcept -> OTNumericHash;

        BitcoinFormat(
            const std::int32_t version,
            const UnallocatedCString& previous,
            const UnallocatedCString& merkle,
            const std::uint32_t time,
            const std::uint32_t nbits,
            const std::uint32_t nonce) noexcept(false);
        BitcoinFormat() noexcept;
    };

    static const VersionNumber local_data_version_;
    static const VersionNumber subversion_default_;

    static auto calculate_hash(
        const api::Session& api,
        const blockchain::Type chain,
        const ReadView serialized) -> blockchain::block::Hash;
    static auto calculate_hash(
        const api::Session& api,
        const blockchain::Type chain,
        const BitcoinFormat& serialized) -> blockchain::block::Hash;
    static auto calculate_pow(
        const api::Session& api,
        const blockchain::Type chain,
        const ReadView serialized) -> blockchain::block::Hash;
    static auto calculate_pow(
        const api::Session& api,
        const blockchain::Type chain,
        const BitcoinFormat& serialized) -> blockchain::block::Hash;

    auto clone() const noexcept
        -> std::unique_ptr<blockchain::block::Header::Imp> final
    {
        return clone_bitcoin();
    }
    auto clone_bitcoin() const noexcept -> std::unique_ptr<Imp> final
    {
        return std::make_unique<Header>(*this);
    }
    auto Encode() const noexcept -> ByteArray final;
    auto MerkleRoot() const noexcept -> const blockchain::block::Hash& final
    {
        return merkle_root_;
    }
    auto Nonce() const noexcept -> std::uint32_t final { return nonce_; }
    auto nBits() const noexcept -> std::uint32_t final { return nbits_; }
    auto Print() const noexcept -> UnallocatedCString final;
    auto Serialize(SerializedType& out) const noexcept -> bool final;
    auto Serialize(
        const AllocateOutput destination,
        const bool bitcoinformat = true) const noexcept -> bool final;
    auto Target() const noexcept -> OTNumericHash final;
    auto Timestamp() const noexcept -> Time final { return timestamp_; }
    auto Version() const noexcept -> std::uint32_t final
    {
        return block_version_;
    }

    Header(
        const api::Session& api,
        const blockchain::Type chain,
        const VersionNumber subversion,
        blockchain::block::Hash&& hash,
        blockchain::block::Hash&& pow,
        const std::int32_t version,
        blockchain::block::Hash&& previous,
        blockchain::block::Hash&& merkle,
        const Time timestamp,
        const std::uint32_t nbits,
        const std::uint32_t nonce,
        const bool isGenesis) noexcept(false);
    Header(
        const api::Session& api,
        const blockchain::Type chain,
        const blockchain::block::Hash& merkle,
        const blockchain::block::Hash& parent,
        const blockchain::block::Height height) noexcept(false);
    Header(const api::Session& api, const SerializedType& serialized) noexcept(
        false);
    Header() = delete;
    Header(const Header& rhs) noexcept;
    Header(Header&&) = delete;
    auto operator=(const Header&) -> Header& = delete;
    auto operator=(Header&&) -> Header& = delete;

    ~Header() final = default;

private:
    using ot_super = blockchain::block::implementation::Header;

    const VersionNumber subversion_;
    const std::int32_t block_version_;
    const blockchain::block::Hash merkle_root_;
    const Time timestamp_;
    const std::uint32_t nbits_;
    const std::uint32_t nonce_;

    static auto calculate_hash(
        const api::Session& api,
        const SerializedType& serialized) -> blockchain::block::Hash;
    static auto calculate_pow(
        const api::Session& api,
        const SerializedType& serialized) -> blockchain::block::Hash;
    static auto calculate_work(
        const blockchain::Type chain,
        const std::uint32_t nbits) -> OTWork;
    static auto preimage(const SerializedType& in) -> BitcoinFormat;

    auto check_pow() const noexcept -> bool;

    auto find_nonce() noexcept(false) -> void;

    Header(
        const api::Session& api,
        const VersionNumber version,
        const blockchain::Type chain,
        blockchain::block::Hash&& hash,
        blockchain::block::Hash&& pow,
        blockchain::block::Hash&& previous,
        const blockchain::block::Height height,
        const Status status,
        const Status inheritStatus,
        const blockchain::Work& work,
        const blockchain::Work& inheritWork,
        const VersionNumber subversion,
        const std::int32_t blockVersion,
        blockchain::block::Hash&& merkle,
        const Time timestamp,
        const std::uint32_t nbits,
        const std::uint32_t nonce,
        const bool validate) noexcept(false);
};
}  // namespace opentxs::blockchain::bitcoin::block::implementation
