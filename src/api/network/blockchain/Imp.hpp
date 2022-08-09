// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <thread>
#include <tuple>

#include "api/network/blockchain/Base.hpp"
#include "api/network/blockchain/Blockchain.hpp"
#include "api/network/blockchain/StartupPublisher.hpp"
#include "blockchain/database/common/Database.hpp"
#include "internal/api/network/Blockchain.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/network/p2p/Client.hpp"
#include "internal/network/p2p/Server.hpp"
#include "internal/network/zeromq/Handle.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/BlockchainHandle.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/network/p2p/Types.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/BlockchainProfile.hpp"
#include "opentxs/util/Container.hpp"
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

namespace network
{
class Blockchain;
}  // namespace network

namespace session
{
class Endpoints;
}  // namespace session

class Legacy;
class Session;
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
namespace internal
{
struct Config;
}  // namespace internal

class Manager;
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace p2p
{
class Client;
class Server;
}  // namespace p2p

namespace zeromq
{
namespace internal
{
class Batch;
class Thread;
}  // namespace internal

namespace socket
{
class Raw;
}  // namespace socket

class Context;
}  // namespace zeromq
}  // namespace network

class Options;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace zmq = opentxs::network::zeromq;

namespace opentxs::api::network::implementation
{
struct BlockchainImp final : public Blockchain::Imp {
    using Imp::AddSyncServer;
    auto AddSyncServer(const std::string_view endpoint) const noexcept
        -> bool final;
    auto BlockAvailableEndpoint() const noexcept -> std::string_view final
    {
        return block_available_endpoint_;
    }
    auto BlockQueueUpdateEndpoint() const noexcept -> std::string_view final
    {
        return block_queue_endpoint_;
    }
    auto ConnectedSyncServers() const noexcept -> Endpoints final;
    auto Database() const noexcept
        -> const opentxs::blockchain::database::common::Database& final
    {
        return *db_;
    }
    auto DeleteSyncServer(const std::string_view endpoint) const noexcept
        -> bool final;
    auto Disable(const Imp::Chain type) const noexcept -> bool final;
    auto Enable(const Imp::Chain type, const std::string_view seednode)
        const noexcept -> bool final;
    auto EnabledChains(alloc::Default) const noexcept -> Set<Imp::Chain> final;
    auto FilterUpdate() const noexcept -> const zmq::socket::Publish& final
    {
        return new_filters_;
    }
    auto Hello(alloc::Default) const noexcept
        -> opentxs::network::p2p::StateData final;
    auto IsEnabled(const Chain chain) const noexcept -> bool final;
    auto GetChain(const Imp::Chain type) const noexcept(false)
        -> BlockchainHandle final;
    auto GetSyncServers(alloc::Default alloc) const noexcept
        -> Imp::Endpoints final;
    auto Mempool() const noexcept -> const zmq::socket::Publish& final
    {
        return mempool_;
    }
    auto PeerUpdate() const noexcept -> const zmq::socket::Publish& final
    {
        return connected_peer_updates_;
    }
    auto Profile() const noexcept -> BlockchainProfile final;
    auto PublishStartup(
        const opentxs::blockchain::Type chain,
        OTZMQWorkType type) const noexcept -> bool final;
    auto Reorg() const noexcept -> const zmq::socket::Publish& final
    {
        return reorg_;
    }
    auto ReportProgress(
        const Chain chain,
        const opentxs::blockchain::block::Height current,
        const opentxs::blockchain::block::Height target) const noexcept
        -> void final;
    auto RestoreNetworks() const noexcept -> void final;
    auto Start(const Imp::Chain type, const std::string_view seednode)
        const noexcept -> bool final;
    auto StartSyncServer(
        const std::string_view syncEndpoint,
        const std::string_view publicSyncEndpoint,
        const std::string_view updateEndpoint,
        const std::string_view publicUpdateEndpoint) const -> bool final;
    auto Stop(const Imp::Chain type) const noexcept -> bool final;
    auto SyncEndpoint() const noexcept -> std::string_view final;
    auto UpdatePeer(
        const opentxs::blockchain::Type chain,
        const std::string_view address) const noexcept -> void final;

    auto Init(
        const api::crypto::Blockchain& crypto,
        const api::Legacy& legacy,
        const std::filesystem::path& dataFolder,
        const Options& args) noexcept -> void final;
    auto Shutdown() noexcept -> void final;

    BlockchainImp(
        const api::Session& api,
        const api::session::Endpoints& endpoints,
        const opentxs::network::zeromq::Context& zmq) noexcept;
    BlockchainImp() = delete;
    BlockchainImp(const BlockchainImp&) = delete;
    BlockchainImp(BlockchainImp&&) = delete;
    auto operator=(const BlockchainImp&) -> BlockchainImp& = delete;
    auto operator=(BlockchainImp&&) -> BlockchainImp& = delete;

    ~BlockchainImp() final;

private:
    using Config = opentxs::blockchain::node::internal::Config;
    using pNode = std::shared_ptr<opentxs::blockchain::node::Manager>;
    using Chains = UnallocatedVector<Chain>;

    const api::Session& api_;
    const api::crypto::Blockchain* crypto_;
    std::unique_ptr<opentxs::blockchain::database::common::Database> db_;
    const UnallocatedCString block_available_endpoint_;
    const UnallocatedCString block_queue_endpoint_;
    opentxs::network::zeromq::internal::Handle handle_;
    opentxs::network::zeromq::internal::Batch& batch_;
    opentxs::network::zeromq::socket::Raw& block_available_out_;
    opentxs::network::zeromq::socket::Raw& block_queue_out_;
    opentxs::network::zeromq::socket::Raw& block_available_in_;
    opentxs::network::zeromq::socket::Raw& block_queue_in_;
    opentxs::network::zeromq::internal::Thread* thread_;
    // TODO move the rest of these publish sockets into the batch. Giving out
    // references to these sockets can cause shutdown race conditions
    OTZMQPublishSocket active_peer_updates_;
    OTZMQPublishSocket chain_state_publisher_;
    OTZMQPublishSocket connected_peer_updates_;
    OTZMQPublishSocket new_filters_;
    OTZMQPublishSocket reorg_;
    OTZMQPublishSocket sync_updates_;
    OTZMQPublishSocket mempool_;
    blockchain::StartupPublisher startup_publisher_;
    std::unique_ptr<Config> base_config_;
    mutable std::mutex lock_;
    mutable UnallocatedMap<Chain, Config> config_;
    mutable UnallocatedMap<Chain, pNode> networks_;
    mutable std::optional<opentxs::network::p2p::Client> sync_client_;
    mutable opentxs::network::p2p::Server sync_server_;
    std::promise<void> init_promise_;
    std::shared_future<void> init_;
    std::atomic_bool running_;

    auto disable(const Lock& lock, const Chain type) const noexcept -> bool;
    auto enable(
        const Lock& lock,
        const Chain type,
        const std::string_view seednode) const noexcept -> bool;
    auto hello(const Lock&, const Chains& chains, alloc::Default alloc)
        const noexcept -> opentxs::network::p2p::StateData;
    auto publish_chain_state(Chain type, bool state) const -> void;
    auto start(
        const Lock& lock,
        const Chain type,
        const std::string_view seednode,
        const bool startWallet = true) const -> bool;
    auto stop(const Lock& lock, const Chain type) const noexcept -> bool;
};
}  // namespace opentxs::api::network::implementation
