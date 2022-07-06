// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/network/zeromq/socket/SocketType.hpp"

#pragma once

#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>

#include "internal/api/network/Asio.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "internal/network/zeromq/socket/Pipeline.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/Asio.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Reactor.hpp"
#include "util/ScopeGuard.hpp"

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

template <typename JobType>
class Actor : public Reactor, virtual public Allocated {
public:
    using Message = network::zeromq::Message;

    auto get_allocator() const noexcept -> allocator_type final {
        return pipeline_.get_allocator();
    }

    const CString &name() const noexcept { return name_; }

protected:
    // This interface used to be private and accessed through friend
    // relationship.
    using Work = JobType;

    virtual auto do_startup() noexcept -> void = 0;

    virtual auto do_shutdown() noexcept -> void = 0;

    virtual auto pipeline(const Work work, Message &&msg) noexcept -> void = 0;

    virtual auto work() noexcept -> bool = 0;

protected:
    auto disable_automatic_processing(bool value) -> void {
        if (value == disable_automatic_processing_.exchange(value)) {
            // No change
            return;
        }
        if (value) {
            // TODO
        } else {
            // TODO
        }
    }

    auto trigger() const noexcept -> void {
        const auto running = state_machine_queued_.exchange(true);

        if (false == running) {
            pipeline_.Push(MakeWork(OT_ZMQ_STATE_MACHINE_SIGNAL));
        }
    }

    auto signal_shutdown(bool immediate = false) noexcept -> void {
        shutdown_actor();
    }

    template<typename ME>
    auto signal_startup(const boost::shared_ptr<ME> &me) noexcept -> void {
        tdiag("signal_startup");
        if (running_ && start()) {
            boost::weak_ptr<ME> wp(me);
            pipeline_.Internal().SetCallback([wp](auto &&m) {
                if (auto also_me = wp.lock()) also_me->worker(std::move(m));
            });
            pipeline_.Push(MakeWork(OT_ZMQ_INIT_SIGNAL));
        }
        notify();
    }

    auto do_init() noexcept -> void {
        log_(name_)(" ")(__FUNCTION__)(": initializing").Flush();
        do_startup();
    }

    auto do_work() noexcept -> void {
        rate_limit_state_machine();
        state_machine_queued_.store(false);
        trigger_repeat_if(work());
    }

protected:
    auto shutdown_actor() noexcept -> void {
        tdiag(typeid(this), "CLOSE 1  XXXXXX");
        tdiag("", static_cast<void *>(this));

        if (in_reactor_thread()) {
            do_shutdown();
            tdiag(typeid(this), "CLOSE 2R XXXXXX");
            tdiag("", static_cast<void *>(this));
            pipeline_.Close();
        } else {
            if (auto previous = running_.exchange(false); previous) {
                if (stop()) {
                    notify();
                    wait_for_loop_exit();
                }
                do_shutdown();
                tdiag(typeid(this), "CLOSE 2A XXXXXX");
                tdiag("", static_cast<void *>(this));
                pipeline_.Close();
            } else {
                tdiag(typeid(this), "CLOSE 3A XXXXXX");
            }
        }
    }

protected:
    Actor(
            const api::Session &api,
            const Log &logger,
            CString &&name,
            const std::chrono::milliseconds rateLimit,
            const network::zeromq::BatchID batch,
            allocator_type alloc,
            const network::zeromq::EndpointArgs &subscribe = {},
            const network::zeromq::EndpointArgs &pull = {},
            const network::zeromq::EndpointArgs &dealer = {},
            const Vector<network::zeromq::SocketData> &extra = {}) noexcept
            : Reactor(logger, name), running_{true}, name_(std::move(name)), log_{logger},
            reorg_lock_(),
              pipeline_{api.Network().ZeroMQ().Internal().Pipeline(
                      {},
                      subscribe,
                      pull,
                      dealer,
                      extra,
                      batch,
                      alloc.resource())}, disable_automatic_processing_{false}, rate_limit_{rateLimit},
              last_executed_{Clock::now()}, cache_(alloc), state_machine_queued_{false}, last_job_{} {
        log_(name_)(" ")(__FUNCTION__)(": using ZMQ batch ")(
                pipeline_.BatchID())
                .Flush();
        tdiag("Actor::Actor");
        notify();
    }

    ~Actor() override { tdiag("Actor::~Actor", processing_thread_id()); }

private:
    auto rate_limit_state_machine() const noexcept {
        const auto wait = std::chrono::duration_cast<std::chrono::microseconds>(
                rate_limit_ - (Clock::now() - last_executed_));

        if (0 < wait.count()) {
            log_(name_)(" ")(__FUNCTION__)(": rate limited for ")(wait.count())(
                    " microseconds")
                    .Flush();
            Sleep(wait);
        }
    }

    auto decode_message_type(const network::zeromq::Message &in) noexcept(false) {
        const auto body = in.Body();

        if (1 > body.size()) {

            throw std::runtime_error{"empty message received"};
        }

        Work work{};
        try {
            work = body.at(0).as<Work>();
            last_job_ = work;
        } catch (const std::out_of_range &e) {  // from FrameSection or
            // deeper from std::vector
            log_(name_)(" ")(__FUNCTION__)(": ")(e.what()).Flush();
            OT_FAIL
        } catch (const std::runtime_error &e) {  // from Frame
            log_(name_)(" ")(__FUNCTION__)(": ")(e.what()).Flush();
            throw;
        } catch (const std::exception &e) {  // possible via copy of
            // template type
            log_(name_)(" ")(__FUNCTION__)(": ")(e.what()).Flush();
            OT_FAIL
        }

        const auto type = print(work);
        log_(name_)(" ")(__FUNCTION__)(": message type is: ")(type).Flush();
        const auto isInit =
                OT_ZMQ_INIT_SIGNAL == static_cast<OTZMQWorkType>(work);

        return std::make_tuple(work, type, isInit);
    }

    auto handle_message(network::zeromq::Message &&in) noexcept -> void {
        try {
            const auto[work, type, isInit] = decode_message_type(in);

            handle_message(  // noexcept
                    false,
                    isInit,
                    type,
                    work,
                    std::move(in));
        } catch (const std::runtime_error &e) {  // re-throw from
            // decode_message_type
            log_(name_)(" ")(__FUNCTION__)(": ")(e.what()).Flush();
            OT_FAIL
        } catch (const std::exception &e) {  // from copy constructors used in
            // structured binding
            log_(name_)(" ")(__FUNCTION__)(": ")(e.what()).Flush();
            OT_FAIL
        }
    }

    auto handle_message(
            const bool topLevel,
            const bool isInit,
            const std::string_view type,
            const Work work,
            network::zeromq::Message &&in) noexcept -> void {
        if (disable_automatic_processing_) {
            log_(name_)(" ")(__FUNCTION__)(": processing ")(
                    type)(" in bypass mode")
                    .Flush();
            handle_message(work, std::move(in));

            return;
        } else if (topLevel) {
            flush_cache();
        }

        switch (static_cast<OTZMQWorkType>(work)) {
            case value(WorkType::Shutdown): {
                log_(name_)(" ")(__FUNCTION__)(": shutting down").Flush();
                this->shutdown_actor();
            }
                break;
            case OT_ZMQ_STATE_MACHINE_SIGNAL: {
                log_(name_)(" ")(__FUNCTION__)(": executing state machine")
                        .Flush();
                do_work();
            }
                break;
            default: {
                log_(name_)(" ")(__FUNCTION__)(": processing ")(type).Flush();
                handle_message(work, std::move(in));
            }
        }
    }

    auto handle_message(const Work work, Message &&msg) noexcept -> void {
        try {
            pipeline(work, std::move(msg));  // move doesn't throw and pipeline
            // is defined as noexcept
        } catch (const std::exception &e) {
            log_(name_)(" ")(__FUNCTION__)(": error processing ")(print(work))(
                    " message: ")(e.what())
                    .Flush();
            OT_FAIL;
        }
    }

    auto trigger_repeat_if(const bool again) noexcept -> void {
        if (again) { trigger(); }

        last_executed_ = Clock::now();
    }

    // The reactor calls to dispatch unqueued messages.
    auto handle(Message &&in) noexcept -> void final {
        try {
            const auto[work, type, isInit] = decode_message_type(in);

            handle_message(true, isInit, type, work, std::move(in));
        } catch (const std::exception &e) {
            log_(name_)(" ")(__FUNCTION__)(": ")(e.what()).Flush();
        }
    }

    // All received messages are queued for the reactor.
    auto worker(Message &&in) noexcept -> void {
        log_(name_)(" ")(__FUNCTION__)(": Message received").Flush();
        enqueue(std::move(in));
    }

    virtual auto to_str(Work) const noexcept -> std::string = 0;

    auto last_job_str() const noexcept -> std::string final
    {
        return to_str(last_job_);
    }

public:
    std::atomic<bool> running_;
    CString name_;

protected:
    using Direction = network::zeromq::socket::Direction;
    using SocketType = network::zeromq::socket::Type;

    const Log& log_;
    mutable std::timed_mutex reorg_lock_;

    network::zeromq::Pipeline pipeline_;

private:
    std::atomic<bool> disable_automatic_processing_;

    const std::chrono::milliseconds rate_limit_;
    Time last_executed_;
    std::queue<Message, Deque<Message>> cache_;
    mutable std::atomic<bool> state_machine_queued_;

    // diagnostic
    Work last_job_;
};
}  // namespace opentxs
