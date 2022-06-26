// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/smart_ptr/shared_ptr.hpp>
#include <memory>
#include <string_view>

#include "internal/blockchain/p2p/bitcoin/Bitcoin.hpp"
#include "internal/network/blockchain/Peer.hpp"

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
namespace database
{
class Peer;
}  // namespace database

namespace node
{
namespace internal
{
class Manager;
class Mempool;
class PeerManager;
struct Config;
}  // namespace internal

class BlockOracle;
class FilterOracle;
class HeaderOracle;
}  // namespace node

namespace p2p
{
namespace internal
{
struct Address;
}  // namespace internal
}  // namespace p2p
}  // namespace blockchain
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::factory
{
auto BlockchainPeerBitcoin(
    const api::Session& api,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Manager& network,
    const blockchain::node::internal::PeerManager& parent,
    const blockchain::node::internal::Mempool& mempool,
    const blockchain::node::HeaderOracle& header,
    const blockchain::node::BlockOracle& block,
    const blockchain::node::FilterOracle& filter,
    const blockchain::p2p::bitcoin::Nonce& nonce,
    blockchain::database::Peer& database,
    int peerID,
    std::unique_ptr<blockchain::p2p::internal::Address> address,
    std::string_view fromParent)
    -> boost::shared_ptr<network::blockchain::internal::Peer::Imp>;
}  // namespace opentxs::factory
