// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <zmq.h>
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <utility>

#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/Thread.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/BoostPMR.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"
#include "util/Gatekeeper.hpp"

struct zmq_pollitem_t;

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace network
{
namespace zeromq
{
namespace internal
{
class Pool;
class Thread;
}  // namespace internal

namespace socket
{
class Raw;
}  // namespace socket

class Message;
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::network::zeromq::context
{
class Thread final : public zeromq::internal::Thread
{
public:
    auto Add(BatchID id, StartArgs&& args) noexcept -> bool;
    auto Alloc() noexcept -> alloc::Resource* final { return &alloc_; }
    auto ID() const noexcept -> std::thread::id final
    {
        return thread_.handle_.get_id();
    }
    auto Modify(SocketID socket, ModifyCallback cb) noexcept
        -> AsyncResult final;
    auto Remove(BatchID id, UnallocatedVector<socket::Raw*>&& sockets) noexcept
        -> std::future<bool>;
    auto Shutdown() noexcept -> void final;

    auto name(const std::string&, bool conditional) noexcept -> void final;

    Thread(zeromq::internal::Pool& parent) noexcept;

    ~Thread() final;

private:
    auto snc_add(ThreadStartArgs&& sockets) noexcept -> bool;
    auto snc_modify(
        SocketID socket,
        ModifyCallback cb,
        std::promise<bool>&&) noexcept -> void;
    auto snc_remove(
        BatchID id,
        UnallocatedVector<socket::Raw*>&& sockets,
        std::promise<bool> p) noexcept -> void;
    auto from_reactor() const noexcept -> bool
    {
        return reactor_id_ == std::this_thread::get_id();
    }

private:
    struct Background {
        std::atomic_bool running_{false};
        std::thread handle_{};
    };
    struct Receivers {
        using SocksVector = Vector<::zmq_pollitem_t>;
        using RxCallbVector = Vector<ReceiveCallback>;

        SocksVector socks_;
        RxCallbVector rxcallbacks_;

        Receivers(alloc::Resource* alloc) noexcept
            : socks_(alloc)
            , rxcallbacks_(alloc)
        {
        }

        ~Receivers();
    };

    zeromq::internal::Pool& parent_;
    std::deque<std::packaged_task<void()>> skt_mgmt_tasks_;
    std::thread::id reactor_id_;
    std::mutex task_mtx_;
    std::condition_variable active_cv_;
    std::mutex active_mtx_;
    std::atomic_bool shutdown_;
    socket::Raw null_skt_;
    alloc::BoostPoolSync alloc_;
    Gatekeeper gate_;
    Background thread_;
    Receivers receivers_;

    auto join() noexcept -> void;
    auto poll(Receivers& data) noexcept -> void;
    auto receive_message(void* socket, Message& message) noexcept -> bool;
    auto run() noexcept -> void;
    auto wait() noexcept -> void;

    Thread() = delete;
    Thread(const Thread&) = delete;
    Thread(Thread&&) = delete;
    auto operator=(const Thread&) -> Thread& = delete;
    auto operator=(Thread&&) -> Thread& = delete;
};
}  // namespace opentxs::network::zeromq::context
