// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                        // IWYU pragma: associated
#include "1_Internal.hpp"                      // IWYU pragma: associated
#include "blockchain/block/header/Header.hpp"  // IWYU pragma: associated

#include <BlockchainBlockHeader.pb.h>  // IWYU pragma: keep
#include <cstdint>
#include <memory>
#include <utility>

#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/Params.hpp"
#include "internal/blockchain/bitcoin/block/Factory.hpp"
#include "internal/blockchain/block/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/bitcoin/NumericHash.hpp"
#include "opentxs/blockchain/bitcoin/Work.hpp"
#include "opentxs/blockchain/bitcoin/block/Header.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::factory
{
auto GenesisBlockHeader(
    const api::Session& api,
    const blockchain::Type type) noexcept
    -> std::unique_ptr<blockchain::block::Header>
{
    switch (type) {
        case blockchain::Type::Bitcoin:
        case blockchain::Type::BitcoinCash:
        case blockchain::Type::Bitcoin_testnet3:
        case blockchain::Type::BitcoinCash_testnet3:
        case blockchain::Type::Litecoin:
        case blockchain::Type::Litecoin_testnet4:
        case blockchain::Type::PKT:
        case blockchain::Type::PKT_testnet:
        case blockchain::Type::BitcoinSV:
        case blockchain::Type::BitcoinSV_testnet3:
        case blockchain::Type::eCash:
        case blockchain::Type::eCash_testnet3:
        case blockchain::Type::UnitTest: {
            const auto& hex =
                blockchain::params::Chains().at(type).genesis_header_hex_;
            const auto data = api.Factory().DataFromHex(hex);

            return factory::BitcoinBlockHeader(api, type, data.Bytes());
        }
        case blockchain::Type::Unknown:
        case blockchain::Type::Ethereum_frontier:
        case blockchain::Type::Ethereum_ropsten:
        case blockchain::Type::Casper:
        case blockchain::Type::Casper_testnet:
        default: {
            LogError()("opentxs::factory::")(__func__)(": Unsupported type (")(
                static_cast<std::uint32_t>(type))(")")
                .Flush();

            return nullptr;
        }
    }
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::block
{
auto Header::Imp::Difficulty() const noexcept -> OTWork
{
    static const auto blank = OTWork{factory::Work("")};

    return blank;
}

auto Header::Imp::Hash() const noexcept -> const block::Hash&
{
    static const auto blank = block::Hash{};

    return blank;
}

auto Header::Imp::IncrementalWork() const noexcept -> OTWork
{
    return Difficulty();
}

auto Header::Imp::NumericHash() const noexcept -> OTNumericHash
{
    static const auto blankData = ByteArray{};
    static const auto blankHash =
        OTNumericHash{factory::NumericHash(blankData)};

    return blankHash;
}

auto Header::Imp::ParentHash() const noexcept -> const block::Hash&
{
    return Hash();
}

auto Header::Imp::ParentWork() const noexcept -> OTWork { return Difficulty(); }

auto Header::Imp::Target() const noexcept -> OTNumericHash
{
    return NumericHash();
}

auto Header::Imp::Work() const noexcept -> OTWork { return Difficulty(); }
}  // namespace opentxs::blockchain::block

namespace opentxs::blockchain::block
{
Header::Header(Imp* imp) noexcept
    : imp_(imp)
{
    OT_ASSERT(nullptr != imp_);
}

Header::Header() noexcept
    : Header(std::make_unique<Header::Imp>().release())
{
}

Header::Header(const Header& rhs) noexcept
    : Header(rhs.imp_->clone().release())
{
}

auto Header::as_Bitcoin() const noexcept
    -> const blockchain::bitcoin::block::Header&
{
    static const auto blank = blockchain::bitcoin::block::Header{};

    return blank;
}

auto Header::clone() const noexcept -> std::unique_ptr<Header>
{
    return std::make_unique<Header>(imp_->clone().release());
}

auto Header::Difficulty() const noexcept -> OTWork
{
    return imp_->Difficulty();
}

auto Header::Hash() const noexcept -> const block::Hash&
{
    return imp_->Hash();
}

auto Header::Height() const noexcept -> block::Height { return imp_->Height(); }

auto Header::IncrementalWork() const noexcept -> OTWork
{
    return imp_->IncrementalWork();
}

auto Header::Internal() const noexcept -> const internal::Header&
{
    return *imp_;
}

auto Header::Internal() noexcept -> internal::Header& { return *imp_; }

auto Header::NumericHash() const noexcept -> OTNumericHash
{
    return imp_->NumericHash();
}

auto Header::ParentHash() const noexcept -> const block::Hash&
{
    return imp_->ParentHash();
}

auto Header::ParentWork() const noexcept -> OTWork
{
    return imp_->ParentWork();
}

auto Header::Position() const noexcept -> block::Position
{
    return imp_->Position();
}

auto Header::Print() const noexcept -> UnallocatedCString
{
    return imp_->Print();
}

auto Header::Serialize(
    const AllocateOutput destination,
    const bool bitcoinformat) const noexcept -> bool
{
    return imp_->Serialize(destination, bitcoinformat);
}

auto Header::swap_header(Header& rhs) noexcept -> void
{
    std::swap(imp_, rhs.imp_);
}

auto Header::Target() const noexcept -> OTNumericHash { return imp_->Target(); }

auto Header::Type() const noexcept -> blockchain::Type { return imp_->Type(); }

auto Header::Valid() const noexcept -> bool { return imp_->Valid(); }

auto Header::Work() const noexcept -> OTWork { return imp_->Work(); }

Header::~Header()
{
    if (nullptr != imp_) {
        delete imp_;
        imp_ = nullptr;
    }
}
}  // namespace opentxs::blockchain::block
