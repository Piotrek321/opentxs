// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <cxxabi.h>

#include "0_stdafx.hpp"                       // IWYU pragma: associated
#include "1_Internal.hpp"                     // IWYU pragma: associated
#include "network/zeromq/context/Thread.hpp"  // IWYU pragma: associated

#include <zmq.h>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

#include "internal/network/zeromq/Pool.hpp"
#include "internal/network/zeromq/socket/Factory.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/Signals.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Time.hpp"
#include "opentxs/util/Types.hpp"
#include "util/ScopeGuard.hpp"
#include "util/threadutil.hpp"

namespace opentxs::network::zeromq::context
{

using namespace std::literals;

Thread::Thread(zeromq::internal::Pool& parent) noexcept
    : parent_(parent)
    , tasks_{}
    , reactor_id_{}
    , task_mtx_{}
    , active_cv_{}
    , active_mtx_{}
    , shutdown_(false)
    , null_skt_(factory::ZMQSocketNull())
    , alloc_()
    , gate_()
    , thread_()
    , receivers_(&alloc_)
{
    thread_.running_ = true;
    thread_.handle_ = std::thread{&Thread::run, this};
}

auto Thread::SetName(std::string_view name) -> bool
{
    return ThreadMonitor::set_name(thread_.handle_.native_handle(), name);
}

auto Thread::Add(BatchID id, StartArgs&& args) noexcept -> bool
{
    const auto ticket = gate_.get();

    if (ticket) { return false; }

    auto sockets = [&] {
        auto out = ThreadStartArgs{};
        std::transform(
            args.begin(), args.end(), std::back_inserter(out), [](auto& val) {
                auto& [sID, socket, cb] = val;

                return std::make_pair(socket, std::move(cb));
            });

        return out;
    }();
    parent_.UpdateIndex(id, std::move(args));

    if (from_reactor()) {
        snc_add(std::move(sockets));
    } else {
        std::unique_lock<std::mutex> task_lock(task_mtx_);
        tasks_.emplace_back([this, skts = std::move(sockets)]() mutable {
            snc_add(std::move(skts));
        });
        active_cv_.notify_one();
    }
    return true;
}

auto Thread::snc_add(ThreadStartArgs&& sockets) noexcept -> bool
{
    for (auto [socket, cb] : sockets) {
        assert(cb);

        receivers_.rxcallbacks_.emplace_back(std::move(cb));
        auto& s = receivers_.socks_.emplace_back();
        s.socket = socket->Native();
        s.events = ZMQ_POLLIN;
    }
    assert(receivers_.socks_.size() == receivers_.rxcallbacks_.size());
    return true;
}

auto Thread::join() noexcept -> void
{
    if (thread_.handle_.joinable()) { thread_.handle_.join(); }
}

auto Thread::Modify(SocketID socket, ModifyCallback cb) noexcept -> AsyncResult
{
    const auto ticket = gate_.get();

    if (ticket) { return {}; }

    if (!cb) {
        std::cerr << (OT_PRETTY_CLASS()) << "invalid callback" << std::endl;

        return {};
    }

    auto prom = std::promise<bool>{};
    auto result = std::make_pair(true, prom.get_future());

    if (from_reactor()) {
        snc_modify(socket, cb, std::move(prom));
    } else {
        std::unique_lock<std::mutex> task_lock(task_mtx_);
        tasks_.emplace_back([this, socket, cb, p = std::move(prom)]() mutable {
            snc_modify(socket, cb, std::move(p));
        });
        active_cv_.notify_one();
    }
    return result;
}

auto Thread::snc_modify(
    SocketID socket,
    ModifyCallback cb,
    std::promise<bool>&& prom) noexcept -> void
{
    if (null_skt_.ID() == socket) {
        try {
            cb(null_skt_);
            prom.set_value(true);
        } catch (...) {
            prom.set_value(false);
        }
    } else {
        prom.set_value(parent_.DoModify(socket, cb));
    }
}

auto Thread::poll(Receivers& data) noexcept -> void
{
    if (shutdown_) {
        thread_.running_ = false;

        return;
    }

    static constexpr auto timeout = 100ms;
    const auto events = ::zmq_poll(
        data.socks_.data(),
        static_cast<int>(data.socks_.size()),
        timeout.count());

    if (0 > events) {
        std::cout << OT_PRETTY_CLASS() << ::zmq_strerror(::zmq_errno())
                  << std::endl;

        return;
    } else if (0 == events) {

        return;
    }

    const auto& v = data.socks_;
    auto c = data.rxcallbacks_.begin();

    for (auto s = v.begin(), end = v.end(); s != end; ++s, ++c) {
        auto& item = *s;

        if (ZMQ_POLLIN != item.revents) { continue; }

        auto& socket = item.socket;
        auto message = Message{};

        if (receive_message(socket, message)) {
            const auto& callback = *c;

            try {
                callback(std::move(message));
            } catch (const std::exception& e) {
                std::cerr << "Thread::poll exception: " << e.what() << "\n";
            }
        }
    }
}

auto Thread::receive_message(void* socket, Message& message) noexcept -> bool
{
    bool receiving{true};

    while (receiving) {
        auto& frame = message.AddFrame();
        const bool received =
            (-1 != ::zmq_msg_recv(frame, socket, ZMQ_DONTWAIT));

        if (false == received) {
            auto zerr = ::zmq_errno();
            if (EAGAIN == zerr) {
                std::cerr
                    << (OT_PRETTY_CLASS())
                    << "zmq_msg_recv returns EAGAIN. This should never happen."
                    << std::endl;
            } else {
                std::cerr << (OT_PRETTY_CLASS())
                          << ": Receive error: " << ::zmq_strerror(zerr)
                          << std::endl;
            }

            return false;
        }

        int option{0};
        std::size_t optionBytes{sizeof(option)};

        const bool haveOption =
            (-1 !=
             ::zmq_getsockopt(socket, ZMQ_RCVMORE, &option, &optionBytes));

        if (false == haveOption) {
            std::cerr << (OT_PRETTY_CLASS())
                      << "Failed to check socket options error:\n"
                      << ::zmq_strerror(zmq_errno()) << std::endl;

            return false;
        }

        assert(optionBytes == sizeof(option));

        if (1 != option) { receiving = false; }
    }

    return true;
}

auto Thread::Remove(BatchID id, UnallocatedVector<socket::Raw*>&& data) noexcept
    -> std::future<bool>
{
    const auto ticket = gate_.get();

    if (ticket) { return {}; }

    auto prom = std::promise<bool>{};
    auto fut = prom.get_future();

    if (from_reactor()) {
        snc_remove(id, std::move(data), std::move(prom));
    } else {
        std::unique_lock<std::mutex> task_lock(task_mtx_);
        tasks_.emplace_back([this,
                             id,
                             sockets = std::move(data),
                             promise = std::move(prom)]() mutable {
            snc_remove(id, std::move(sockets), std::move(promise));
        });
        active_cv_.notify_one();
    }
    return fut;
}

auto Thread::snc_remove(
    BatchID id,
    UnallocatedVector<socket::Raw*>&& sockets,
    std::promise<bool> prom) noexcept -> void
{
    const auto set = [&] {
        auto out = UnallocatedSet<void*>{};
        std::transform(
            sockets.begin(),
            sockets.end(),
            std::inserter(out, out.end()),
            [](auto* socket) { return socket->Native(); });

        return out;
    }();
    auto s = receivers_.socks_.begin();
    auto c = receivers_.rxcallbacks_.begin();

    while ((s != receivers_.socks_.end()) &&
           (c != receivers_.rxcallbacks_.end())) {
        auto* socket = s->socket;

        if (0u == set.count(socket)) {
            ++s;
            ++c;
        } else {
            s = receivers_.socks_.erase(s);
            c = receivers_.rxcallbacks_.erase(c);
        }
    }

    assert(receivers_.socks_.size() == receivers_.rxcallbacks_.size());

    parent_.UpdateIndex(id);
    prom.set_value(true);
}

auto Thread::run() noexcept -> void
{
    Signals::Block();
    try {
        reactor_id_ = std::this_thread::get_id();
        while (thread_.running_) {
            {
                // Optionally do externally called tasks like connections
                // or sockets additions.
                std::unique_lock<std::mutex> task_lock(task_mtx_);
                if (!tasks_.empty()) {
                    tasks_.front()();
                    tasks_.pop_front();
                    continue;
                }
            }

            poll(receivers_);

            {
                // Block while there are no sockets.
                std::unique_lock<std::mutex> active_lock(active_mtx_);
                if (thread_.running_ && receivers_.rxcallbacks_.empty()) {
                    active_cv_.wait(active_lock);
                }
            }
        }
        // Dispose of outstanding tasks unconditionally.
        while (!tasks_.empty()) {
            tasks_.front()();
            tasks_.pop_front();
        }

    } catch (const std::exception& e) {
        std::cerr << ThreadMonitor::get_name()
                  << "Thread::run( exception: " << e.what() << "\n";
        exit(0);
    }
}

auto Thread::Shutdown() noexcept -> void
{
    gate_.shutdown();
    {
        std::unique_lock<std::mutex> active_lock(active_mtx_);
        shutdown_ = true;
        thread_.running_ = false;
        active_cv_.notify_one();
    }
    join();
    receivers_.socks_.clear();
    receivers_.rxcallbacks_.clear();
}

Thread::Receivers::~Receivers() = default;

Thread::~Thread() { Shutdown(); }
}  // namespace opentxs::network::zeromq::context
