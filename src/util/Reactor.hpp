// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/network/zeromq/socket/SocketType.hpp"

#pragma once

#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <cs_libguarded/src/cs_plain_guarded.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string_view>
#include <utility>

#include "internal/network/zeromq/Types.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Log.hpp"
#include "util/ScopeGuard.hpp"
#include "util/threadutil.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{

namespace api
{
class Session;
}  // namespace api

class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs
{

// Classes used internally for synchronization.
// TODO Consider redefining as private or protected in Reactor,
//    to limit the visibility.

class Command
{
public:
    Command() = default;
    virtual ~Command() {}
    virtual auto exec() -> void = 0;
};

template <typename F>
class SynchronizableCommand : public Command
{
public:
    explicit SynchronizableCommand(F f)
        : Command{}
        , task_{f}
    {
    }
    auto exec() -> void final { task_(); }
    auto get_future() { return task_.get_future(); }

private:
    using R = typename std::invoke_result<F>::type;
    std::packaged_task<R()> task_;
};
template <typename F>
SynchronizableCommand(F) -> SynchronizableCommand<F>;

class Reactor : public ThreadDisplay
{
public:
    // Run a functor in the reactor thread.
    // This is intended to be taking a lambda with an appropriate
    // capture.
    template <typename Functor>
    auto synchronize(Functor&& f)
    {
        if (in_reactor_thread()) { return f(); }
        tdiag(typeid(this), "synchronize command");
        std::unique_ptr<SynchronizableCommand<Functor>> fc(
            new SynchronizableCommand(std::move(f)));
        auto ft = fc->get_future();
        {
            auto cq = command_queue_.lock();
            cq->push(std::move(fc));
            queue_event_flag_.notify_one();
        }
        return ft.get();
    }

protected:
    // Handle a message in a subclass.
    virtual auto handle(network::zeromq::Message&& in) noexcept -> void = 0;

    // Add a message to reactor queue.
    auto enqueue(network::zeromq::Message&& in) -> bool;

    // Start the reactor thread.
    bool start();

    // Open reactor loop for exit.
    bool stop();

    // Check if the caller is the reactor loop.
    bool in_reactor_thread() const noexcept;

    // Say something may have been added to a queue.
    auto notify() -> void;

    // Wait for the reactor loop to exit after stop().
    auto wait_for_loop_exit() const -> void;

    // Submitting a message for deferred handling.
    auto defer(network::zeromq::Message&& in) -> void;

    // This is handling all deferred messages.
    auto flush_cache() -> void;

private:
    // for diagnostics only
    virtual auto last_job_str() const noexcept -> std::string = 0;

private:
    using Promises = libguarded::plain_guarded<std::deque<std::promise<void>>>;
    std::atomic<bool> active_;
    Promises deferred_promises_;
    mutable std::mutex queue_event_mtx_;
    mutable std::condition_variable queue_event_flag_;
    std::unique_ptr<std::thread> thread_;
    std::promise<void> promise_shutdown_;
    std::shared_future<void> future_shutdown_;

protected:
    using CommandQueue =
        libguarded::plain_guarded<std::queue<std::unique_ptr<Command>>>;
    using MessageQueue =
        libguarded::plain_guarded<std::queue<network::zeromq::Message>>;
    std::thread::id processing_thread_id();

private:
    CommandQueue command_queue_;
    MessageQueue message_queue_;
    MessageQueue deferred_queue_;

    const Log& log_;
    CString name_;

private:
    auto dequeue_message() -> std::optional<network::zeromq::Message>;

    auto dequeue_deferred() -> std::optional<network::zeromq::Message>;

    auto dequeue_command() noexcept -> std::unique_ptr<Command>;

    auto try_post(network::zeromq::Message&& m) noexcept
        -> std::optional<network::zeromq::Message>;

    auto process_deferred() -> void;

protected:
    Reactor(const Log& logger, const CString& name) noexcept;
    ~Reactor() override;

private:
    std::thread::id processing_thread_id_;

private:
    int reactor_loop();
};
}  // namespace opentxs
