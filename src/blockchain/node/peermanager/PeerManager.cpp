// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/peermanager/PeerManager.hpp"  // IWYU pragma: associated

#include <chrono>
#include <memory>
#include <optional>

#include "core/Worker.hpp"
#include "internal/api/network/Blockchain.hpp"
#include "internal/blockchain/bitcoin/block/Transaction.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/p2p/P2P.hpp"  // IWYU pragma: keep
#include "internal/util/LogMacros.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/block/Block.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/p2p/Address.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::factory
{
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
    const blockchain::database::BlockStorage policy,
    const UnallocatedCString& seednode,
    const UnallocatedCString& shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::PeerManager>
{
    using ReturnType = blockchain::node::implementation::PeerManager;

    return std::make_unique<ReturnType>(
        api,
        config,
        mempool,
        node,
        headers,
        filter,
        block,
        database,
        type,
        policy,
        seednode,
        shutdown);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node
{

auto print(PeerManagerJobs job) noexcept -> std::string_view
{
    try {
        static const auto map = Map<PeerManagerJobs, std::string>{
            {node::PeerManagerJobs::Shutdown, "Shutdown"},
            {node::PeerManagerJobs::Mempool, "Mempool"},
            {node::PeerManagerJobs::Register, "Register"},
            {node::PeerManagerJobs::Connect, "Connect"},
            {node::PeerManagerJobs::Disconnect, "Disconnect"},
            {node::PeerManagerJobs::P2P, "P2P"},
            {node::PeerManagerJobs::Getheaders, "Getheaders"},
            {node::PeerManagerJobs::Getblock, "Getblock"},
            {node::PeerManagerJobs::BroadcastTransaction,
             "BroadcastTransaction"},
            {node::PeerManagerJobs::BroadcastBlock, "BroadcastBlock"},
            {node::PeerManagerJobs::JobAvailableCfheaders,
             "JobAvailableCfheaders"},
            {node::PeerManagerJobs::JobAvailableCfilters,
             "JobAvailableCfilters"},
            {node::PeerManagerJobs::JobAvailableBlock, "JobAvailableBlock"},
            {node::PeerManagerJobs::ActivityTimeout, "ActivityTimeout"},
            {node::PeerManagerJobs::NeedPing, "NeedPing"},
            {node::PeerManagerJobs::Body, "Body"},
            {node::PeerManagerJobs::Header, "Header"},
            {node::PeerManagerJobs::Heartbeat, "Heartbeat"},
            {node::PeerManagerJobs::Init, "Init"},
            {node::PeerManagerJobs::ReceiveMessage, "ReceiveMessage"},
            {node::PeerManagerJobs::SendMessage, "SendMessage"},
            {node::PeerManagerJobs::StateMachine, "StateMachine"},
        };

        return map.at(job);
    } catch (...) {
        LogError()(__FUNCTION__)("invalid PeerManagerJobs: ")(
            static_cast<OTZMQWorkType>(job))
            .Flush();

        OT_FAIL;
    }
}
}  // namespace opentxs::blockchain::node

namespace opentxs::blockchain::node::implementation
{
auto PeerManager::to_str(Work value) -> std::string
{
    static auto Map = std::map<Work, std::string>{
        {Work::Disconnect, "Disconnect"},
        {Work::AddPeer, "AddPeer"},
        {Work::AddListener, "AddListener"},
        {Work::IncomingPeer, "IncomingPeer"},
        {Work::StateMachine, "StateMachine"},
        {Work::Shutdown, "Shutdown"}};
    auto i = Map.find(value);
    return i == Map.end() ? std::string{"???"} : i->second;
}

PeerManager::PeerManager(
    const api::Session& api,
    const internal::Config& config,
    const node::internal::Mempool& mempool,
    const internal::Manager& node,
    const HeaderOracle& headers,
    const internal::FilterOracle& filter,
    const internal::BlockOracle& block,
    database::Peer& database,
    const Type chain,
    const database::BlockStorage policy,
    const UnallocatedCString& seednode,
    const UnallocatedCString& shutdown) noexcept
    : internal::PeerManager()
    , Worker(api, "PeerManager")
    , closed_{}
    , node_(node)
    , database_(database)
    , chain_(chain)
    , peer_target_(peer_target(chain_, policy))
    , jobs_(api)
    , peers_(
          api,
          config,
          mempool,
          node_,
          headers,
          filter,
          block,
          database_,
          *this,
          policy,
          shutdown,
          chain,
          seednode,
          peer_target_)
    , verified_lock_()
    , verified_peers_()
    , init_promise_()
    , init_(init_promise_.get_future())
    , last_job_{}
{
    init_executor({shutdown});
    start();
}

auto PeerManager::AddIncomingPeer(const int id, std::uintptr_t endpoint)
    const noexcept -> void
{
    auto work = MakeWork(Work::IncomingPeer);
    work.AddFrame(id);
    work.AddFrame(endpoint);
    pipeline_.Push(std::move(work));
}

auto PeerManager::AddPeer(
    const blockchain::p2p::Address& address) const noexcept -> bool
{
    if (!running_.load()) { return false; }

    auto address_p = std::make_unique<OTBlockchainAddress>(address);
    auto promise = std::make_unique<std::promise<bool>>();
    auto future = promise->get_future();
    auto work = MakeWork(Work::AddPeer);
    work.AddFrame(reinterpret_cast<std::uintptr_t>(address_p.release()));
    work.AddFrame(reinterpret_cast<std::uintptr_t>(promise.release()));
    pipeline_.Push(std::move(work));

    while (running_.load()) {
        if (std::future_status::ready == future.wait_for(5s)) {

            return future.get();
        }
    }

    return false;
}

auto PeerManager::BroadcastBlock(const block::Block& block) const noexcept
    -> bool
{
    if (!running_.load()) { return false; }

    if (0 == peers_.Count()) {
        LogError()(OT_PRETTY_CLASS())("no peers available").Flush();

        return false;
    }
    auto work = jobs_.Work(PeerManagerJobs::BroadcastBlock);
    work.AddFrame(block.ID());

    tdiag("about to dispatch BroadcastBlock");
    jobs_.Dispatch(std::move(work));

    return true;
}

auto PeerManager::BroadcastTransaction(
    const bitcoin::block::Transaction& tx) const noexcept -> bool
{
    if (!running_.load()) { return false; }

    if (0 == peers_.Count()) { return false; }

    auto bytes = Space{};

    if (!tx.Internal().Serialize(writer(bytes)).has_value()) { return false; }

    const auto view = reader(bytes);
    auto work = jobs_.Work(PeerManagerJobs::BroadcastTransaction);
    work.AddFrame(view.data(), view.size());
    tdiag("BroadcastTransaction dispatch");
    jobs_.Dispatch(std::move(work));

    return true;
}

auto PeerManager::Connect() noexcept -> bool
{
    if (!running_.load()) { return false; }

    trigger();

    return true;
}

auto PeerManager::Disconnect(const int id) const noexcept -> void
{
    auto work = MakeWork(Work::Disconnect);
    work.AddFrame(id);
    pipeline_.Push(std::move(work));
}

auto PeerManager::GetVerifiedPeerCount() const noexcept -> std::size_t
{
    auto lock = Lock{verified_lock_};

    return verified_peers_.size();
}

auto PeerManager::init() noexcept -> void
{
    init_promise_.set_value();
    trigger();
}

auto PeerManager::JobReady(const PeerManagerJobs type) const noexcept -> void
{
    switch (type) {
        case PeerManagerJobs::JobAvailableCfheaders: {
            jobs_.Dispatch(jobs_.Work(PeerManagerJobs::JobAvailableCfheaders));
        } break;
        case PeerManagerJobs::JobAvailableCfilters: {
            jobs_.Dispatch(jobs_.Work(PeerManagerJobs::JobAvailableCfilters));
        } break;
        case PeerManagerJobs::JobAvailableBlock: {
            jobs_.Dispatch(jobs_.Work(PeerManagerJobs::JobAvailableBlock));
        } break;
        default: {
        }
    }
}

auto PeerManager::Listen(const blockchain::p2p::Address& address) const noexcept
    -> bool
{
    if (!running_.load()) { return false; }

    auto address_p = std::make_unique<OTBlockchainAddress>(address);
    auto promise = std::make_unique<std::promise<bool>>();
    auto future = promise->get_future();
    auto work = MakeWork(Work::AddListener);
    work.AddFrame(reinterpret_cast<std::uintptr_t>(address_p.release()));
    work.AddFrame(reinterpret_cast<std::uintptr_t>(promise.release()));
    pipeline_.Push(std::move(work));

    while (running_.load()) {
        if (std::future_status::ready == future.wait_for(10s)) {

            return future.get();
        } else {

            return false;
        }
    }

    return false;
}

auto PeerManager::LookupIncomingSocket(const int id) const noexcept(false)
    -> opentxs::network::asio::Socket
{
    return peers_.LookupIncomingSocket(id);
}

auto PeerManager::peer_target(
    const Type chain,
    const database::BlockStorage policy) noexcept -> std::size_t
{
    if (Type::UnitTest == chain) { return 0; }

    switch (policy) {
        case database::BlockStorage::All: {

            return 6;
        }
        case database::BlockStorage::Cache: {

            return 4;
        }
        case database::BlockStorage::None:
        default: {

            return 2;
        }
    }
}

auto PeerManager::pipeline(zmq::Message&& message) -> void
{
    if (!running_.load()) { return; }

    const auto body = message.Body();

    OT_ASSERT(0 < body.size());

    const auto work = body.at(0).as<Work>();
    last_job_ = work;

    switch (work) {
        case Work::Disconnect: {
            tdiag("PeerManager::Disconnect");
            OT_ASSERT(1 < body.size());

            const auto id = body.at(1).as<int>();

            {
                auto lock = Lock{verified_lock_};
                verified_peers_.erase(id);
            }

            peers_.Disconnect(id);
            api_.Network().Blockchain().Internal().UpdatePeer(chain_, "");
            do_work();
        } break;
        case Work::AddPeer: {
            OT_ASSERT(2 < body.size());

            using Promise = std::promise<bool>;

            auto address_p = std::unique_ptr<OTBlockchainAddress>{
                reinterpret_cast<OTBlockchainAddress*>(
                    body.at(1).as<std::uintptr_t>())};
            auto promise_p = std::unique_ptr<Promise>{
                reinterpret_cast<Promise*>(body.at(2).as<std::uintptr_t>())};

            OT_ASSERT(address_p);
            OT_ASSERT(promise_p);

            const auto& address = address_p->get();
            auto& promise = *promise_p;

            peers_.AddPeer(address, promise);
            do_work();
        } break;
        case Work::AddListener: {
            OT_ASSERT(2 < body.size());

            using Promise = std::promise<bool>;

            auto address_p = std::unique_ptr<OTBlockchainAddress>{
                reinterpret_cast<OTBlockchainAddress*>(
                    body.at(1).as<std::uintptr_t>())};
            auto promise_p = std::unique_ptr<Promise>{
                reinterpret_cast<Promise*>(body.at(2).as<std::uintptr_t>())};

            OT_ASSERT(address_p);
            OT_ASSERT(promise_p);

            const auto& address = address_p->get();
            auto& promise = *promise_p;

            peers_.AddListener(address, promise);
            do_work();
        } break;
        case Work::IncomingPeer: {
            OT_ASSERT(2 < body.size());

            const auto id = body.at(1).as<int>();
            auto endpoint = Peers::Endpoint{
                reinterpret_cast<blockchain::p2p::internal::Address*>(
                    body.at(2).as<std::uintptr_t>())};

            OT_ASSERT(0 <= id);
            OT_ASSERT(endpoint);

            peers_.AddIncoming(id, std::move(endpoint));
            do_work();
        } break;
        case Work::StateMachine: {
            do_work();
        } break;
        case Work::Shutdown: {
            protect_shutdown([this] { shut_down(); });
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto PeerManager::RequestBlock(const block::Hash& block) const noexcept -> bool
{
    if (block.empty()) { return false; }

    return RequestBlocks({block.Bytes()});
}

auto PeerManager::RequestBlocks(
    const UnallocatedVector<ReadView>& hashes) const noexcept -> bool
{
    if (!running_.load()) { return false; }

    if (0 == peers_.Count()) { return false; }

    if (hashes.empty()) { return false; }

    auto work = jobs_.Work(PeerManagerJobs::Getblock);
    static constexpr auto limit =
        std::size_t{1u};  // TODO peers don't respond well to larger values

    for (const auto& block : hashes) {
        work.AddFrame(block.data(), block.size());

        if (work.Body().size() > limit) {
            tdiag("RequestBlocks about to dispatch Getblock");
            jobs_.Dispatch(std::move(work));
            work = jobs_.Work(PeerManagerJobs::Getblock);
        }
    }

    if (work.Body().size() > 1u) {
        tdiag("RequestBlocks about to dispatch unknown");
        jobs_.Dispatch(std::move(work));
    }

    return true;
}

auto PeerManager::RequestHeaders() const noexcept -> bool
{
    if (!running_.load()) { return false; }

    if (0 == peers_.Count()) { return false; }

    tdiag("RequestHeaders about to dispatch Getheaders");
    jobs_.Dispatch(PeerManagerJobs::Getheaders);

    return true;
}

auto PeerManager::shut_down() noexcept -> void
{
    if (!closed_.exchange(true)) {
        close_pipeline();
        tdiag("Peers Shutdown");
        peers_.Shutdown();
        tdiag("jobs Shutdown");
        jobs_.Shutdown();
    }
}

auto PeerManager::state_machine() noexcept -> int
{
    LogTrace()(OT_PRETTY_CLASS()).Flush();

    if (!running_.load()) { return -1; }

    return peers_.Run();
}

auto PeerManager::VerifyPeer(const int id, const UnallocatedCString& address)
    const noexcept -> void
{
    {
        auto lock = Lock{verified_lock_};
        verified_peers_.emplace(id);
    }

    api_.Network().Blockchain().Internal().UpdatePeer(chain_, address);
}

auto PeerManager::last_job_str() const noexcept -> std::string
{
    return to_str(last_job_);
}

PeerManager::~PeerManager()
{
    protect_shutdown([this] { shut_down(); });
}
}  // namespace opentxs::blockchain::node::implementation
