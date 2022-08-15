// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <future>
#include <mutex>
#include <optional>

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
#include "util/Reactor.hpp"
#include "util/Work.hpp"

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
class Worker : public Reactor
{
protected:
    virtual auto pipeline(network::zeromq::Message&& in) -> void = 0;
    virtual auto state_machine() noexcept -> int = 0;

protected:
    using Endpoints = UnallocatedVector<UnallocatedCString>;

    const API& api_;
    std::string diagnostic_;
    std::atomic<bool> running_;

private:
    std::promise<void> shutdown_promise_;
    std::shared_future<void> shutdown_complete_;
    std::mutex shutdown_mutex_;
    Time last_executed_;
    mutable std::atomic<bool> state_machine_queued_;
    //    std::once_flag thread_register_once_;

protected:
    network::zeromq::Pipeline pipeline_;

    auto trigger() const noexcept -> void
    {
        if (!running_) { return; }

        const auto was_queued = state_machine_queued_.exchange(true);

        if (!was_queued) {
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
        if (!running_) { return; }

        state_machine_queued_ = false;
        int when_next = state_machine();
        if (when_next == 0) {
            last_executed_ = Clock::now();
            trigger();
        } else if (when_next > 0) {
            last_executed_ = Clock::now();
            trigger_at(last_executed_ + std::chrono::milliseconds(when_next));
        }
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
        std::string diagnostic,
        const Vector<network::zeromq::SocketData>& extra = {}) noexcept
        : Reactor(LogTrace(), diagnostic)
        , api_{api}
        , diagnostic_{diagnostic}
        , running_{true}
        , shutdown_promise_{}
        , shutdown_complete_{shutdown_promise_.get_future()}
        , shutdown_mutex_{}
        , last_executed_{Clock::now()}
        , state_machine_queued_{}
        , pipeline_{api.Network().ZeroMQ().Internal().Pipeline(
              std::move(diagnostic),
              [this](network::zeromq::Message&& in) { post(std::move(in)); },
              {},
              {},
              {},
              extra)}
    {
        LogTrace()(OT_PRETTY_CLASS())("using ZMQ batch ")(pipeline_.BatchID())
            .Flush();
        //        start();
        tdiag("created ", name());
    }

    ~Worker() override
    {
        tdiag("Worker::~Worker.1");
        protect_shutdown([this] { close_pipeline(); });
        tdiag("Worker::~Worker.2");
    }

    auto is_running() const noexcept { return running_.load(); }

    void protect_shutdown(std::function<void()> funct)
    {
        bool can_do = running_.exchange(false);
        if (can_do) {
            synchronize([this, funct]() {
                stop();
                funct();
                close_pipeline();
            });
            shutdown_promise_.set_value();
        } else {
            stop();
        }
    }

private:
    auto trigger_at(
        std::chrono::time_point<std::chrono::system_clock> t_at) noexcept
        -> void
    {
        auto queued = state_machine_queued_.exchange(true);

        if (!queued) { post_at(MakeWork(OT_ZMQ_STATE_MACHINE_SIGNAL), t_at); }
    }
    auto handle(network::zeromq::Message&& in, unsigned) noexcept -> void final
    {
        if (running_) pipeline(std::move(in));
    }
};
}  // namespace opentxs
