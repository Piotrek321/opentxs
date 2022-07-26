// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>

#include "opentxs/util/Container.hpp"

namespace opentxs::blockchain::p2p
{
namespace bitcoin
{
using ProtocolVersion = std::int32_t;
using ProtocolVersionUnsigned = std::uint32_t;
}  // namespace bitcoin

enum class Network : std::uint8_t {
    ipv6 = 0,
    ipv4 = 1,
    onion2 = 2,
    onion3 = 3,
    eep = 4,
    cjdns = 5,
    zmq = 6,
};
enum class Protocol : std::uint8_t {
    opentxs = 0,
    bitcoin = 1,
    ethereum = 2,
    casper = 3,
};

enum class Service : std::uint8_t {
    None = 0,
    Avalanche = 1,
    BitcoinCash = 2,
    Bloom = 3,
    CompactFilters = 4,
    Graphene = 5,
    Limited = 6,
    Network = 7,
    Segwit2X = 8,
    UTXO = 9,
    WeakBlocks = 10,
    Witness = 11,
    XThin = 12,
    XThinner = 13,
};

OPENTXS_EXPORT auto DisplayService(const Service service) noexcept
    -> UnallocatedCString;
}  // namespace opentxs::blockchain::p2p
