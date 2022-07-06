// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>

#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "util/Work.hpp"
#include "util/threadutil.hpp"
#include "util/Reactor.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace session
{
class Client;
}  // namespace session
}  // namespace api
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs
{
template <typename API = api::session::Client>
class Worker
{
protected:
    virtual auto pipeline(network::zeromq::Message&& in) -> void = 0;
    virtual auto state_machine() noexcept -> bool = 0;

protected:
    using Endpoints = UnallocatedVector<UnallocatedCString>;

    const API& api_;
    const std::chrono::milliseconds rate_limit_;
    std::atomic<bool> running_;

private:
    std::promise<void> shutdown_promise_;
    std::shared_future<void> shutdown_complete_;
    std::mutex shutdown_mutex_;
    Time last_executed_;
    mutable std::atomic<bool> state_machine_queued_;

protected:
    network::zeromq::Pipeline pipeline_;

    auto trigger() const noexcept -> void
    {
        if (false == running_.load()) { return; }

        const auto running = state_machine_queued_.exchange(true);

        if (false == running) {
            pipeline_.Push(MakeWork(OT_ZMQ_STATE_MACHINE_SIGNAL));
        }
    }
    auto signal_shutdown() noexcept -> std::shared_future<void>
    {
        protect_shutdown([this] { close_pipeline(); });
        return shutdown_complete_;
    }
    auto close_pipeline() noexcept -> void { pipeline_.Close(); }

    auto do_work() noexcept
    {
        rate_limit_state_machine();
        state_machine_queued_.store(false);
        if (state_machine()) { trigger(); }
        last_executed_ = Clock::now();
    }
    auto init_executor(const Endpoints endpoints = {}) noexcept -> void
    {
        pipeline_.SubscribeTo(api_.Endpoints().Shutdown());

        for (const auto& endpoint : endpoints) {
            pipeline_.SubscribeTo(endpoint);
        }
    }

    Worker(
        const API& api,
        const std::chrono::milliseconds rateLimit,
        const Vector<network::zeromq::SocketData>& extra = {}) noexcept
        : api_{api}
        , rate_limit_{rateLimit}
        , running_{true}
        , shutdown_promise_{}
        , shutdown_complete_{shutdown_promise_.get_future()}
        , shutdown_mutex_{}
        , last_executed_{Clock::now()}
        , state_machine_queued_{false}
        , pipeline_{api.Network().ZeroMQ().Internal().Pipeline(
              [this](auto&& in) {
                  if (running_) pipeline(std::move(in));
              },
              {},
              {},
              {},
              extra)}
    {
        LogTrace()(OT_PRETTY_CLASS())("using ZMQ batch ")(pipeline_.BatchID())
            .Flush();
    }

    virtual ~Worker()
    {
        protect_shutdown([this] { close_pipeline(); });
    }

    auto is_running() const noexcept { return running_.load(); }

    void protect_shutdown(std::function<void()> funct)
    {
        bool running = true;

        // Note: the lock is to hold up callers who will not get to
        // trigger another shutdown functor execution but who could
        // instead proceed to complete object destruction.
        std::unique_lock<std::mutex> lock(shutdown_mutex_);

        // Note: despite mutex protection, we use atomic here to avoid
        // having to block the execution of the working thread.
        bool can_do = running_.compare_exchange_strong(running, false);

        if (can_do) {
            funct();
            shutdown_promise_.set_value();
        }
    }

private:
    auto rate_limit_state_machine() const noexcept
    {
        auto outstanding =
            rate_limit_ - std::chrono::duration_cast<std::chrono::milliseconds>(
                              Clock::now() - last_executed_);
        if (0 < outstanding.count()) {
            std::this_thread::sleep_for(outstanding);
        }
    }
};
}  // namespace opentxs
