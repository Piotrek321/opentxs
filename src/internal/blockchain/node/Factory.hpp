// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <string_view>

#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace crypto
{
class Blockchain;
}  // namespace crypto

namespace network
{
class Blockchain;
}  // namespace network

class Session;
}  // namespace api

namespace blockchain
{
namespace database
{
class Block;
class Cfilter;
class Header;
class Peer;
class Wallet;
}  // namespace database

namespace node
{
namespace internal
{
class BlockOracle;
class FilterOracle;
class Mempool;
class Manager;
class PeerManager;
class Wallet;
struct Config;
}  // namespace internal

class HeaderOracle;
}  // namespace node
}  // namespace blockchain
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::factory
{
auto BlockchainFilterOracle(
    const api::Session& api,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::HeaderOracle& header,
    const blockchain::node::internal::BlockOracle& block,
    blockchain::database::Cfilter& database,
    const blockchain::Type chain,
    const blockchain::cfilter::Type filter,
    const UnallocatedCString& shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::FilterOracle>;
auto BlockchainNetworkBitcoin(
    const api::Session& api,
    const blockchain::Type type,
    const blockchain::node::internal::Config& config,
    std::string_view seednode,
    std::string_view syncEndpoint) noexcept
    -> std::unique_ptr<blockchain::node::internal::Manager>;
auto BlockchainPeerManager(
    const api::Session& api,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Mempool& mempool,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::HeaderOracle& headers,
    const blockchain::node::internal::FilterOracle& filter,
    const blockchain::node::internal::BlockOracle& block,
    blockchain::database::Peer& database,
    const blockchain::Type type,
    std::string_view seednode,
    std::string_view shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::PeerManager>;
auto BlockchainWallet(
    const api::Session& api,
    const blockchain::node::internal::Manager& parent,
    blockchain::database::Wallet& db,
    const blockchain::node::internal::Mempool& mempool,
    const blockchain::Type chain,
    const std::string_view shutdown)
    -> std::unique_ptr<blockchain::node::internal::Wallet>;
auto BlockOracle(
    const api::Session& api,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::internal::Config& config,
    const blockchain::node::HeaderOracle& header,
    blockchain::database::Block& db,
    const blockchain::Type chain,
    const UnallocatedCString& shutdown) noexcept
    -> blockchain::node::internal::BlockOracle;
auto HeaderOracle(
    const api::Session& api,
    blockchain::database::Header& database,
    const blockchain::Type type) noexcept
    -> std::unique_ptr<blockchain::node::HeaderOracle>;
}  // namespace opentxs::factory
