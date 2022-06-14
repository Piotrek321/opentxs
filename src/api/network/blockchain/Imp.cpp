// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                    // IWYU pragma: associated
#include "1_Internal.hpp"                  // IWYU pragma: associated
#include "api/network/blockchain/Imp.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <iterator>
#include <utility>

#include "blockchain/database/common/Database.hpp"
#include "internal/api/session/Endpoints.hpp"
#include "internal/blockchain/Params.hpp"
#include "internal/blockchain/database/Types.hpp"
#include "internal/blockchain/node/Config.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/network/zeromq/Batch.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/blockchain/p2p/Types.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ZeroMQ.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Options.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Work.hpp"
#include "util/threadutil.hpp"

namespace opentxs::api::network
{
BlockchainImp::BlockchainImp(
    const api::Session& api,
    const api::session::Endpoints& endpoints,
    const opentxs::network::zeromq::Context& zmq) noexcept
    : api_(api)
    , crypto_(nullptr)
    , db_(nullptr)
    , block_available_endpoint_(opentxs::network::zeromq::MakeArbitraryInproc())
    , block_queue_endpoint_(opentxs::network::zeromq::MakeArbitraryInproc())
    , handle_(zmq.Internal().MakeBatch([&] {
        using Type = opentxs::network::zeromq::socket::Type;
        auto out = Vector<Type>{};
        out.emplace_back(Type::Publish);  // NOTE block_available_out_
        out.emplace_back(Type::Publish);  // NOTE block_queue_out_
        out.emplace_back(Type::Pull);     // NOTE block_available_in_
        out.emplace_back(Type::Pull);     // NOTE block_queue_in_

        return out;
    }()))
    , batch_(handle_.batch_)
    , block_available_out_([&]() -> auto& {
        auto& socket = batch_.sockets_.at(0);
        const auto endpoint =
            UnallocatedCString{endpoints.BlockchainBlockAvailable()};
        const auto rc = socket.Bind(endpoint.c_str());

        OT_ASSERT(rc);

        return socket;
    }())
    , block_queue_out_([&]() -> auto& {
        auto& socket = batch_.sockets_.at(1);
        const auto endpoint =
            UnallocatedCString{endpoints.BlockchainBlockDownloadQueue()};
        const auto rc = socket.Bind(endpoint.c_str());

        OT_ASSERT(rc);

        return socket;
    }())
    , block_available_in_([&]() -> auto& {
        auto& socket = batch_.sockets_.at(2);
        const auto rc = socket.Bind(block_available_endpoint_.c_str());

        OT_ASSERT(rc);

        return socket;
    }())
    , block_queue_in_([&]() -> auto& {
        auto& socket = batch_.sockets_.at(3);
        const auto rc = socket.Bind(block_queue_endpoint_.c_str());

        OT_ASSERT(rc);

        return socket;
    }())
    , thread_(zmq.Internal().Start(
          batch_.id_,
          {
              {block_available_in_.ID(),
               &block_available_in_,
               [socket = &block_available_out_](auto&& m) {
                   socket->SendDeferred(std::move(m));
               }},
              {block_queue_in_.ID(),
               &block_queue_in_,
               [socket = &block_queue_out_](auto&& m) {
                   socket->SendDeferred(std::move(m));
               }},
          }))
    , active_peer_updates_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainPeer().data());

        OT_ASSERT(listen);

        return out;
    }())
    , chain_state_publisher_([&] {
        auto out = zmq.PublishSocket();
        auto rc = out->Start(endpoints.BlockchainStateChange().data());

        OT_ASSERT(rc);

        return out;
    }())
    , connected_peer_updates_([&] {
        auto out = zmq.PublishSocket();
        const auto listen =
            out->Start(endpoints.BlockchainPeerConnection().data());

        OT_ASSERT(listen);

        return out;
    }())
    , new_filters_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainNewFilter().data());

        OT_ASSERT(listen);

        return out;
    }())
    , reorg_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainReorg().data());

        OT_ASSERT(listen);

        return out;
    }())
    , sync_updates_([&] {
        auto out = zmq.PublishSocket();
        const auto listen =
            out->Start(endpoints.BlockchainSyncProgress().data());

        OT_ASSERT(listen);

        return out;
    }())
    , mempool_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainMempool().data());

        OT_ASSERT(listen);

        return out;
    }())
    , startup_publisher_(endpoints, zmq)
    , base_config_(nullptr)
    , lock_()
    , config_()
    , networks_()
    , sync_client_(std::nullopt)
    , sync_server_(api_, zmq)
    , init_promise_()
    , init_(init_promise_.get_future())
    , running_(true)
{
    OT_ASSERT(nullptr != thread_);
}

auto BlockchainImp::AddSyncServer(
    const UnallocatedCString& endpoint) const noexcept -> bool
{
    init_.get();

    return db_->AddSyncServer(endpoint);
}

auto BlockchainImp::AddSyncServer(std::string_view endpoint) const noexcept
    -> bool
{
    return AddSyncServer(UnallocatedCString{endpoint});
}

auto BlockchainImp::ConnectedSyncServers() const noexcept -> Endpoints
{
    return {};  // TODO
}

auto BlockchainImp::DeleteSyncServer(
    const UnallocatedCString& endpoint) const noexcept -> bool
{
    init_.get();

    return db_->DeleteSyncServer(endpoint);
}

auto BlockchainImp::Disable(const Chain type) const noexcept -> bool
{
    auto lock = Lock{lock_};

    return disable(lock, type);
}

auto BlockchainImp::disable(const Lock& lock, const Chain type) const noexcept
    -> bool
{
    if (0 == opentxs::blockchain::SupportedChains().count(type)) {
        LogError()(OT_PRETTY_CLASS())("Unsupported chain").Flush();

        return false;
    }

    stop(lock, type);

    if (db_->Disable(type)) { return true; }

    LogError()(OT_PRETTY_CLASS())("Database update failure").Flush();

    return false;
}

auto BlockchainImp::Enable(const Chain type, const UnallocatedCString& seednode)
    const noexcept -> bool
{
    auto lock = Lock{lock_};

    return enable(lock, type, seednode);
}

auto BlockchainImp::enable(
    const Lock& lock,
    const Chain type,
    const UnallocatedCString& seednode) const noexcept -> bool
{
    if (0 == opentxs::blockchain::SupportedChains().count(type)) {
        LogError()(OT_PRETTY_CLASS())("Unsupported chain").Flush();

        return false;
    }

    init_.get();

    if (false == db_->Enable(type, seednode)) {
        LogError()(OT_PRETTY_CLASS())("Database error").Flush();

        return false;
    }

    return start(lock, type, seednode);
}

auto BlockchainImp::EnabledChains() const noexcept -> UnallocatedSet<Chain>
{
    UnallocatedSet<Chain> out{};
    init_.get();
    UnallocatedVector<
        ::opentxs::blockchain::database::common::Database::EnabledChain>
        data;
    {
        auto lock = Lock{lock_};
        data = db_->LoadEnabledChains();
    }
    std::transform(
        data.begin(),
        data.end(),
        std::inserter(out, out.begin()),
        [](const auto value) { return value.first; });

    return out;
}

auto BlockchainImp::GetChain(const Chain type) const noexcept(false)
    -> const opentxs::blockchain::node::Manager&
{
    auto lock = Lock{lock_};

    return *networks_.at(type);
}

auto BlockchainImp::GetSyncServers() const noexcept -> Endpoints
{
    init_.get();

    return db_->GetSyncServers();
}

auto BlockchainImp::Hello() const noexcept -> SyncData
{
    auto lock = Lock{lock_};

    Chains chains{};
    chains.reserve(networks_.size());
    for (const auto& [chain, network] : networks_) {
        chains.emplace_back(chain);
    }

    return hello(lock, chains);
}

auto BlockchainImp::hello(const Lock&, const Chains& chains) const noexcept
    -> SyncData
{
    auto output = SyncData{};

    for (const auto chain : chains) {
        const auto& network = networks_.at(chain);
        const auto& filter = network->FilterOracle();
        const auto type = filter.DefaultType();
        output.emplace_back(chain, filter.FilterTip(type));
    }

    return output;
}

auto BlockchainImp::Init(
    const api::crypto::Blockchain& crypto,
    const api::Legacy& legacy,
    const UnallocatedCString& dataFolder,
    const Options& options) noexcept -> void
{
    crypto_ = &crypto;
    db_ = std::make_unique<opentxs::blockchain::database::common::Database>(
        api_, crypto, legacy, dataFolder, options);

    OT_ASSERT(db_);

    auto sync = (0 < options.RemoteBlockchainSyncServers().size()) ||
                options.ProvideBlockchainSyncServer();

    using Policy = opentxs::blockchain::database::BlockStorage;
    base_config_ = std::make_unique<Config>();
    if (Policy::All == db_->BlockPolicy()) {
        base_config_->generate_cfilters_ = true;

        if (sync) {
            base_config_->provide_sync_server_ = true;
            base_config_->disable_wallet_ = true;
        }
    } else if (sync || (false == options.TestMode())) {
        base_config_->use_sync_server_ = true;

    } else {
        base_config_->download_cfilters_ = true;
    }

    base_config_->disable_wallet_ = !options.BlockchainWalletEnabled();

    if (base_config_->use_sync_server_) { sync_client_.emplace(api_); }

    init_promise_.set_value();

    static const auto defaultServers = Vector<CString>{
        "tcp://metier1.opentransactions.org:8814",
        "tcp://metier2.opentransactions.org:8814",
    };

    const auto servers = GetSyncServers();
    // TODO GetSyncServers should return pmr strings
    Set<CString> existing(servers.begin(), servers.end());

    for (const auto& server : defaultServers) {
        if (0 == existing.count(server)) {
            if (false == api_.GetOptions().TestMode()) {
                AddSyncServer(server);
            }
            existing.emplace(server);
        }
    }

    try {
        for (const auto& endpoint : options.RemoteBlockchainSyncServers()) {
            if (0 == existing.count(endpoint)) { AddSyncServer(endpoint); }
        }
    } catch (...) {
    }
}

auto BlockchainImp::IsEnabled(
    const opentxs::blockchain::Type chain) const noexcept -> bool
{
    init_.get();
    auto lock = Lock{lock_};

    for (const auto& [enabled, peer] : db_->LoadEnabledChains()) {
        if (chain == enabled) { return true; }
    }

    return false;
}

auto BlockchainImp::publish_chain_state(Chain type, bool state) const -> void
{
    auto work = opentxs::network::zeromq::tagged_message(
        WorkType::BlockchainStateChange);
    work.AddFrame(type);
    work.AddFrame(state);

    chain_state_publisher_->Send(std::move(work));
}

auto BlockchainImp::PublishStartup(
    const opentxs::blockchain::Type chain,
    OTZMQWorkType type) const noexcept -> bool
{
    using Dir = opentxs::network::zeromq::socket::Direction;
    auto push = api_.Network().ZeroMQ().PushSocket(Dir::Connect);
    auto out =
        push->Start(api_.Endpoints().Internal().BlockchainStartupPull().data());

    auto work = MakeWork(type);
    work.AddFrame(chain);

    out &= push->Send(std::move(work));
    return out;
}

auto BlockchainImp::ReportProgress(
    const Chain chain,
    const opentxs::blockchain::block::Height current,
    const opentxs::blockchain::block::Height target) const noexcept -> void
{
    auto work = opentxs::network::zeromq::tagged_message(
        WorkType::BlockchainSyncProgress);
    work.AddFrame(chain);
    work.AddFrame(current);
    work.AddFrame(target);
    sync_updates_->Send(std::move(work));
}

auto BlockchainImp::RestoreNetworks() const noexcept -> void
{
    init_.get();

    if (sync_client_) { sync_client_->Init(api_.Network().Blockchain()); }

    auto lock = Lock{lock_};

    for (const auto& [chain, peer] : db_->LoadEnabledChains()) {
        start(lock, chain, peer, false);
    }

    for (auto& [chain, pNode] : networks_) { pNode->StartWallet(); }
}

auto BlockchainImp::Shutdown() noexcept -> void
{
    if (running_.exchange(false)) {
        LogVerbose()("Shutting down ")(networks_.size())(" blockchain clients")
            .Flush();

        for (auto& [chain, network] : networks_) { network->Shutdown().get(); }

        networks_.clear();
    }

    Imp::Shutdown();
}

auto BlockchainImp::Start(const Chain type, const UnallocatedCString& seednode)
    const noexcept -> bool
{
    auto lock = Lock{lock_};

    return start(lock, type, seednode);
}

auto BlockchainImp::start(
    const Lock& lock,
    const Chain type,
    const UnallocatedCString& seednode,
    const bool startWallet) const -> bool
{
    init_.get();

    if (Chain::UnitTest != type) {
        if (0 == opentxs::blockchain::SupportedChains().count(type)) {
            LogError()(OT_PRETTY_CLASS())("Unsupported chain").Flush();

            return false;
        }
    }

    if (0 != networks_.count(type)) {
        LogVerbose()(OT_PRETTY_CLASS())("Chain already running").Flush();

        return true;
    } else {
        LogConsole()("Starting ")(print(type))(" client").Flush();
    }

    namespace p2p = opentxs::blockchain::p2p;

    switch (opentxs::blockchain::params::Chains().at(type).p2p_protocol_) {
        case p2p::Protocol::bitcoin: {
            auto endpoint = UnallocatedCString{};
            if (!base_config_)
                throw std::runtime_error("Base config not initialized!");

            if (base_config_->provide_sync_server_) {
                sync_server_.Enable(type);
                endpoint = sync_server_.Endpoint(type);
            }

            const auto& config =
                config_.try_emplace(type, *base_config_).first->second;
            auto [iter, added] = networks_.emplace(
                type,
                factory::BlockchainNetworkBitcoin(
                    api_, type, config, seednode, endpoint));
            LogConsole()(print(type))(" client is running").Flush();
            publish_chain_state(type, true);
            auto& node = *(iter->second);

            if (startWallet) { node.StartWallet(); }

            return node.Connect();
        }
        case p2p::Protocol::opentxs:
        case p2p::Protocol::ethereum:
        default: {
        }
    }

    return false;
}

auto BlockchainImp::StartSyncServer(
    const UnallocatedCString& sync,
    const UnallocatedCString& publicSync,
    const UnallocatedCString& update,
    const UnallocatedCString& publicUpdate) const -> bool
{
    auto lock = Lock{lock_};
    if (!base_config_) throw std::runtime_error("Base config not initialized!");

    if (base_config_->provide_sync_server_) {
        return sync_server_.Start(sync, publicSync, update, publicUpdate);
    }

    LogConsole()(
        "Blockchain sync server must be enabled at library "
        "initialization time by using Options::SetBlockchainSyncEnabled.")
        .Flush();

    return false;
}

auto BlockchainImp::Stop(const Chain type) const noexcept -> bool
{
    auto lock = Lock{lock_};

    return stop(lock, type);
}

auto BlockchainImp::stop(const Lock& lock, const Chain type) const noexcept
    -> bool
{
    auto it = networks_.find(type);

    if (networks_.end() == it) { return true; }

    OT_ASSERT(it->second);

    sync_server_.Disable(type);
    it->second->Shutdown().get();
    networks_.erase(it);
    LogVerbose()(OT_PRETTY_CLASS())("stopped chain ")(print(type)).Flush();
    publish_chain_state(type, false);

    return true;
}

auto BlockchainImp::SyncEndpoint() const noexcept -> std::string_view
{
    if (sync_client_) {

        return sync_client_->Endpoint();
    } else {

        return Imp::SyncEndpoint();
    }
}

auto BlockchainImp::UpdatePeer(
    const opentxs::blockchain::Type chain,
    const UnallocatedCString& address) const noexcept -> void
{
    auto work = MakeWork(WorkType::BlockchainPeerAdded);
    work.AddFrame(chain);
    work.AddFrame(address);
    active_peer_updates_->Send(std::move(work));
}

BlockchainImp::~BlockchainImp() { batch_.ClearCallbacks(); }
}  // namespace opentxs::api::network
