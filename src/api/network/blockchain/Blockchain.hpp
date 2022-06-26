// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <functional>
#include <stdexcept>
#include <string_view>

#include "internal/api/network/Blockchain.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/network/p2p/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/BlockchainProfile.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/WorkType.hpp"

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

class Legacy;
}  // namespace api

namespace blockchain
{
namespace database
{
namespace common
{
class Database;
}  // namespace common
}  // namespace database

namespace node
{
class Manager;
}  // namespace node
}  // namespace blockchain

namespace identifier
{
class Nym;
}  // namespace identifier

namespace network
{
namespace zeromq
{
namespace socket
{
class Publish;
}  // namespace socket
}  // namespace zeromq
}  // namespace network

class Options;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace zmq = opentxs::network::zeromq;

namespace opentxs::api::network
{
struct Blockchain::Imp : virtual public internal::Blockchain {
    using Chain = network::Blockchain::Chain;
    using Endpoints = network::Blockchain::Endpoints;

    virtual auto AddSyncServer(
        [[maybe_unused]] const std::string_view endpoint) const noexcept -> bool
    {
        return {};
    }
    auto BlockAvailableEndpoint() const noexcept -> std::string_view override
    {
        return {};
    }
    auto BlockQueueUpdateEndpoint() const noexcept -> std::string_view override
    {
        return {};
    }
    virtual auto ConnectedSyncServers() const noexcept -> Endpoints
    {
        return {};
    }
    auto Database() const noexcept
        -> const opentxs::blockchain::database::common::Database& override
    {
        OT_FAIL;
    }
    virtual auto DeleteSyncServer(
        [[maybe_unused]] const std::string_view endpoint) const noexcept -> bool
    {
        return {};
    }
    virtual auto Disable([[maybe_unused]] const Chain type) const noexcept
        -> bool
    {
        return {};
    }
    virtual auto Enable(
        [[maybe_unused]] const Chain type,
        [[maybe_unused]] const std::string_view seednode) const noexcept -> bool
    {
        return {};
    }
    virtual auto EnabledChains(alloc::Default) const noexcept -> Set<Chain>
    {
        return {};
    }
    auto FilterUpdate() const noexcept -> const zmq::socket::Publish& override
    {
        OT_FAIL;
    }
    /// throws std::out_of_range if chain has not been started
    virtual auto GetChain([[maybe_unused]] const Chain type) const
        noexcept(false) -> const opentxs::blockchain::node::Manager&
    {
        throw std::out_of_range("no blockchain support");
    }
    virtual auto GetSyncServers(alloc::Default) const noexcept -> Endpoints
    {
        return {};
    }
    auto Hello(alloc::Default) const noexcept
        -> opentxs::network::p2p::StateData override
    {
        return {};
    }
    auto IsEnabled([[maybe_unused]] const Chain chain) const noexcept
        -> bool override
    {
        return {};
    }
    auto Mempool() const noexcept -> const zmq::socket::Publish& override
    {
        OT_FAIL;
    }
    auto PeerUpdate() const noexcept -> const zmq::socket::Publish& override
    {
        OT_FAIL;
    }
    virtual auto Profile() const noexcept -> BlockchainProfile { return {}; }
    auto PublishStartup(const opentxs::blockchain::Type, OTZMQWorkType)
        const noexcept -> bool override
    {
        return false;
    }
    auto Reorg() const noexcept -> const zmq::socket::Publish& override
    {
        OT_FAIL;
    }
    auto ReportProgress(
        [[maybe_unused]] const Chain,
        [[maybe_unused]] const opentxs::blockchain::block::Height,
        [[maybe_unused]] const opentxs::blockchain::block::Height)
        const noexcept -> void override
    {
    }
    auto RestoreNetworks() const noexcept -> void override {}
    virtual auto Start(
        [[maybe_unused]] const Chain type,
        [[maybe_unused]] const std::string_view seednode) const noexcept -> bool
    {
        return {};
    }
    virtual auto StartSyncServer(
        [[maybe_unused]] const std::string_view syncEndpoint,
        [[maybe_unused]] const std::string_view publicSyncEndpoint,
        [[maybe_unused]] const std::string_view updateEndpoint,
        [[maybe_unused]] const std::string_view publicUpdateEndpoint)
        const -> bool
    {
        return {};
    }
    virtual auto Stop([[maybe_unused]] const Chain type) const noexcept -> bool
    {
        return {};
    }
    auto SyncEndpoint() const noexcept -> std::string_view override
    {
        return {};
    }
    auto UpdatePeer(
        [[maybe_unused]] const opentxs::blockchain::Type,
        [[maybe_unused]] const std::string_view) const noexcept -> void override
    {
    }

    auto Init(
        [[maybe_unused]] const api::crypto::Blockchain& crypto,
        [[maybe_unused]] const api::Legacy& legacy,
        [[maybe_unused]] const std::string_view dataFolder,
        [[maybe_unused]] const Options& args) noexcept -> void override
    {
    }
    auto Shutdown() noexcept -> void override {}

    Imp() = default;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~Imp() override = default;
};
}  // namespace opentxs::api::network
