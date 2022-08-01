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

#include "blockchain/block/header/Header.hpp"
#include "blockchain/block/header/Imp.hpp"
#include "internal/blockchain/bitcoin/block/Header.hpp"
#include "internal/blockchain/block/Block.hpp"
//#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
//#include "opentxs/blockchain/bitcoin/NumericHash.hpp"
//#include "opentxs/blockchain/bitcoin/Work.hpp"
#include "opentxs/blockchain/bitcoin/block/Header.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"
#include "opentxs/util/Time.hpp"

namespace opentxs
{

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
class Hash;
}  // namespace block
}  // namespace blockchain

class ByteArray;
// }  // namespace v1
}  // namespace opentxs

namespace opentxs::blockchain::bitcoin::block
{
class Header::Imp : virtual public blockchain::block::Header::Imp
{
public:
    virtual auto clone_bitcoin() const noexcept -> std::unique_ptr<Imp>
    {
        return std::make_unique<Imp>();
    }

    virtual auto MerkleRoot() const noexcept -> const blockchain::block::Hash&;
    virtual auto Encode() const noexcept -> ByteArray;
    virtual auto Nonce() const noexcept -> std::uint32_t { return {}; }
    virtual auto nBits() const noexcept -> std::uint32_t { return {}; }
    virtual auto Timestamp() const noexcept -> Time { return {}; }
    virtual auto Version() const noexcept -> std::uint32_t { return {}; }

    ~Imp() override = default;
};
}  // namespace opentxs::blockchain::bitcoin::block
