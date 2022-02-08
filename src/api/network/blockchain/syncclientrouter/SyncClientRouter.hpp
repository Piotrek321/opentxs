// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/blockchain/BlockchainType.hpp"
// IWYU pragma: no_include "opentxs/network/zeromq/socket/SocketType.hpp"

#pragma once

#include <cs_deferred_guarded.h>
#include <atomic>
#include <cstddef>
#include <random>
#include <shared_mutex>
#include <string_view>

#include "api/network/blockchain/syncclientrouter/Server.hpp"
#include "internal/api/network/blockchain/SyncClientRouter.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/Timer.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Work.hpp"

namespace opentxs
{
namespace api
{
namespace network
{
class Blockchain;
}  // namespace network

class Session;
}  // namespace api

namespace network
{
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

class ListenCallback;
class Message;
}  // namespace zeromq
}  // namespace network
}  // namespace opentxs

class opentxs::api::network::blockchain::SyncClientRouter::Imp
{
public:
    using Chain = opentxs::blockchain::Type;
    using Callback = opentxs::network::zeromq::ListenCallback;
    using SocketType = opentxs::network::zeromq::socket::Type;

    auto Endpoint() const noexcept -> std::string_view;

    auto Init(const Blockchain& parent) noexcept -> void;

    Imp(const api::Session& api,
        opentxs::network::zeromq::internal::Batch& batch) noexcept;

    ~Imp();

private:
    using ServerMap = Map<CString, syncclientrouter::Server>;
    using ChainMap = Map<Chain, CString>;
    using ProviderMap = Map<Chain, UnallocatedSet<CString>>;
    using ActiveMap = Map<Chain, std::atomic<std::size_t>>;
    using Height = opentxs::blockchain::block::Height;
    using HeightMap = Map<Chain, Height>;
    using Message = opentxs::network::zeromq::Message;
    using GuardedSocket = libguarded::deferred_guarded<
        opentxs::network::zeromq::socket::Raw,
        std::shared_mutex>;

    const api::Session& api_;
    const CString endpoint_;
    const CString monitor_endpoint_;
    const CString loopback_endpoint_;
    opentxs::network::zeromq::internal::Batch& batch_;
    const Callback& external_cb_;
    const Callback& internal_cb_;
    const Callback& monitor_cb_;
    opentxs::network::zeromq::socket::Raw& external_router_;
    opentxs::network::zeromq::socket::Raw& monitor_;
    opentxs::network::zeromq::socket::Raw& external_sub_;
    opentxs::network::zeromq::socket::Raw& internal_router_;
    opentxs::network::zeromq::socket::Raw& internal_sub_;
    opentxs::network::zeromq::socket::Raw& loopback_;
    mutable GuardedSocket to_loopback_;
    mutable std::random_device rd_;
    mutable std::default_random_engine eng_;
    syncclientrouter::Server blank_;
    Timer timer_;
    HeightMap progress_;
    ServerMap servers_;
    ChainMap clients_;
    ProviderMap providers_;
    ActiveMap active_;
    UnallocatedSet<CString> connected_servers_;
    std::atomic<std::size_t> connected_count_;
    std::atomic_bool running_;
    opentxs::network::zeromq::internal::Thread* thread_;

    auto get_chain(Chain chain) const noexcept -> CString;
    auto get_provider(Chain chain) const noexcept -> CString;
    auto get_required_height(Chain chain) const noexcept -> Height;

    auto ping_server(syncclientrouter::Server& server) noexcept -> void;
    auto process_header(Message&& msg) noexcept -> void;
    auto process_external(Message&& msg) noexcept -> void;
    auto process_internal(Message&& msg) noexcept -> void;
    auto process_monitor(Message&& msg) noexcept -> void;
    auto process_register(Message&& msg) noexcept -> void;
    auto process_request(Message&& msg) noexcept -> void;
    auto process_server(Message&& msg) noexcept -> void;
    auto process_server(const CString ep) noexcept -> void;
    auto reset_timer() noexcept -> void;
    auto server_is_active(syncclientrouter::Server& server) noexcept -> void;
    auto server_is_stalled(syncclientrouter::Server& server) noexcept -> void;
    auto shutdown() noexcept -> void;
    auto startup(const Blockchain& parent) noexcept -> void;
    auto state_machine() noexcept -> void;

    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;
};