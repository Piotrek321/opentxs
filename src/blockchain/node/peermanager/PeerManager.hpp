// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/vector.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include "1_Internal.hpp"
#include "blockchain/node/peermanager/Peers.hpp"
#include "core/Worker.hpp"
#include "internal/blockchain/node/PeerManager.hpp"
#include "internal/blockchain/node/Types.hpp"
#include "internal/blockchain/p2p/P2P.hpp"
#include "internal/blockchain/p2p/bitcoin/Bitcoin.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/p2p/Types.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/network/asio/Socket.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Time.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Gatekeeper.hpp"
#include "util/Work.hpp"

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
class Transaction;
}  // namespace block
}  // namespace bitcoin

namespace database
{
class Peer;
}  // namespace database

namespace node
{
namespace internal
{
class Mempool;
struct Config;
}  // namespace internal

namespace peermanager
{
class Peers;
}  // namespace peermanager

class BlockOracle;
class FilterOracle;
class HeaderOracle;
class Manager;
}  // namespace node

namespace p2p
{
class Address;
}  // namespace p2p
}  // namespace blockchain

namespace network
{
namespace asio
{
class Socket;
}  // namespace asio

namespace zeromq
{
namespace socket
{
class Sender;
}  // namespace socket

class Context;
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace zmq = opentxs::network::zeromq;

namespace opentxs::blockchain::node::implementation
{
class PeerManager final : virtual public node::internal::PeerManager,
                          public Worker<api::Session>
{
public:
    enum class Work : OTZMQWorkType {
        Disconnect = OT_ZMQ_INTERNAL_SIGNAL + 0,
        AddPeer = OT_ZMQ_INTERNAL_SIGNAL + 1,
        AddListener = OT_ZMQ_INTERNAL_SIGNAL + 2,
        IncomingPeer = OT_ZMQ_INTERNAL_SIGNAL + 3,
        StateMachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
        Shutdown = value(WorkType::Shutdown),
    };

    auto AddIncomingPeer(const int id, std::uintptr_t endpoint) const noexcept
        -> void final;
    auto AddPeer(const blockchain::p2p::Address& address) const noexcept
        -> bool final;
    auto BroadcastTransaction(
        const bitcoin::block::Transaction& tx) const noexcept -> bool final;
    auto Connect() noexcept -> bool final;
    auto Disconnect(const int id) const noexcept -> void final;
    auto Endpoint(const PeerManagerJobs type) const noexcept
        -> UnallocatedCString final
    {
        return jobs_.Endpoint(type);
    }
    auto GetPeerCount() const noexcept -> std::size_t final
    {
        return peers_.Count();
    }
    auto GetVerifiedPeerCount() const noexcept -> std::size_t final;
    auto Heartbeat() const noexcept -> void final
    {
        jobs_.Dispatch(PeerManagerJobs::Heartbeat);
    }
    auto JobReady(const PeerManagerJobs type) const noexcept -> void final;
    auto Listen(const blockchain::p2p::Address& address) const noexcept
        -> bool final;
    auto LookupIncomingSocket(const int id) const noexcept(false)
        -> opentxs::network::asio::Socket final;
    auto Nonce() const noexcept -> const blockchain::p2p::bitcoin::Nonce& final
    {
        return peers_.Nonce();
    }
    auto RequestBlock(const block::Hash& block) const noexcept -> bool final;
    auto RequestBlocks(const UnallocatedVector<ReadView>& hashes) const noexcept
        -> bool final;
    auto RequestHeaders() const noexcept -> bool final;
    auto VerifyPeer(const int id, const UnallocatedCString& address)
        const noexcept -> void final;

    auto Shutdown() noexcept -> void final
    {
        protect_shutdown([this] { shut_down(); });
    }

    auto Start() noexcept -> void override;

    PeerManager(
        const api::Session& api,
        const node::internal::Config& config,
        const node::internal::Mempool& mempool,
        const node::Manager& node,
        const node::HeaderOracle& headers,
        const node::FilterOracle& filter,
        const node::BlockOracle& block,
        database::Peer& database,
        const Type chain,
        std::string_view seednode,
        std::string_view shutdown) noexcept;
    PeerManager() = delete;
    PeerManager(const PeerManager&) = delete;
    PeerManager(PeerManager&&) = delete;
    auto operator=(const PeerManager&) -> PeerManager& = delete;
    auto operator=(PeerManager&&) -> PeerManager& = delete;

    ~PeerManager() final;

protected:
    auto pipeline(zmq::Message&& message) -> void final;
    auto state_machine() noexcept -> bool final;

private:
    auto shut_down() noexcept -> void;

    struct Jobs {
        auto Endpoint(const PeerManagerJobs type) const noexcept
            -> UnallocatedCString;
        auto Work(const PeerManagerJobs task) const noexcept
            -> network::zeromq::Message;

        auto Dispatch(const PeerManagerJobs type) noexcept -> void;
        auto Dispatch(zmq::Message&& work) -> void;
        auto Shutdown() noexcept -> void;

        Jobs(const api::Session& api) noexcept;
        Jobs() = delete;

    private:
        using EndpointMap = UnallocatedMap<PeerManagerJobs, UnallocatedCString>;
        using SocketMap = UnallocatedMap<PeerManagerJobs, zmq::socket::Sender*>;

        const zmq::Context& zmq_;
        OTZMQPushSocket getheaders_;
        OTZMQPublishSocket getcfheaders_;
        OTZMQPublishSocket getcfilters_;
        OTZMQPublishSocket getblocks_;
        OTZMQPublishSocket heartbeat_;
        OTZMQPushSocket getblock_;
        OTZMQPushSocket broadcast_transaction_;
        const EndpointMap endpoint_map_;
        const SocketMap socket_map_;

        static auto listen(
            EndpointMap& map,
            const PeerManagerJobs type,
            const zmq::socket::Sender& socket) noexcept -> void;
    };

    const node::Manager& node_;
    database::Peer& database_;
    const Type chain_;
    mutable Jobs jobs_;
    mutable peermanager::Peers peers_;
    mutable std::mutex verified_lock_;
    mutable UnallocatedSet<int> verified_peers_;
    std::promise<void> init_promise_;
    std::shared_future<void> init_;

    auto shutdown(std::promise<void>& promise) noexcept -> void;
};
}  // namespace opentxs::blockchain::node::implementation
