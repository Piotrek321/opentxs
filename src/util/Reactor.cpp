// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/network/zeromq/socket/SocketType.hpp"

#include "util/Reactor.hpp"

namespace opentxs
{

std::thread::id Reactor::processing_thread_id()
{
    return processing_thread_id_;
}

auto Reactor::enqueue(network::zeromq::Message&& in) -> bool
{
    if (active_) {
        {
            auto mq = message_queue_.lock();
            mq->push(in);
        }
        queue_event_flag_.notify_all();
        return true;
    }
    return false;
}

auto Reactor::defer(network::zeromq::Message&& in) -> void
{
    auto mq = deferred_queue_.lock();
    mq->push(in);
}

auto Reactor::dequeue_message() -> std::optional<network::zeromq::Message>
{
    std::optional<network::zeromq::Message> msg{};
    auto mq = message_queue_.lock();
    if (!mq->empty()) {
        msg = std::move(mq->front());
        mq->pop();
    }
    return msg;
}

auto Reactor::dequeue_deferred() -> std::optional<network::zeromq::Message>
{
    std::optional<network::zeromq::Message> msg{};
    auto mq = deferred_queue_.lock();
    if (!mq->empty()) {
        msg = std::move(mq->front());
        mq->pop();
    }
    return msg;
}

auto Reactor::dequeue_command() noexcept -> std::unique_ptr<Command>
{
    auto cq = command_queue_.lock();
    if (!cq->empty()) {
        auto c = std::move(cq->front());
        cq->pop();
        return c;
    }
    return {};
}

auto Reactor::try_post(network::zeromq::Message&& m) noexcept
    -> std::optional<network::zeromq::Message>
{
    if (in_reactor_thread()) {
        auto reject(std::move(m));
        return std::move(reject);
    }
    {
        auto mq = message_queue_.lock();
        mq->push(m);
    }
    queue_event_flag_.notify_one();
    return {};
}

auto Reactor::process_deferred() -> void
{
    while (active_) {
        auto m = dequeue_deferred();
        if (!m.has_value()) { break; }
        time_it(
            [&] { handle(std::move(m.value())); },
            "XX DEFERRED MESSAGE HANDLER",
            500);
    }
}

auto Reactor::flush_cache() -> void
{
    if (in_reactor_thread()) {
        process_deferred();
    } else {
        std::promise<void> p{};
        auto f = p.get_future();
        {
            auto deferred_promises_handle = deferred_promises_.lock();
            deferred_promises_handle->push_back(std::move(p));
        }
        notify();
        f.get();
    }
}

bool Reactor::start()
{

    if (active_.exchange(true)) return false;

    thread_.reset(new std::thread(&Reactor::reactor_loop, this));

    return true;
}

bool Reactor::stop() { return active_.exchange(false); }

auto Reactor::notify() -> void { queue_event_flag_.notify_one(); }

auto Reactor::wait_for_loop_exit() const -> void
{
    try {
        auto f = future_shutdown_;
        f.get();
    } catch (const std::exception& e) {
        log_(name_)(" ")(__FUNCTION__)(": ")(e.what()).Flush();
        throw;
    }
}

bool Reactor::in_reactor_thread() const noexcept
{
    return thread_ && thread_->get_id() == std::this_thread::get_id();
}

Reactor::Reactor(const Log& logger, const CString& name) noexcept
    : ThreadDisplay()
    , active_{}
    , deferred_promises_{}
    , queue_event_mtx_{}
    , queue_event_flag_{}
    , thread_{}
    , promise_shutdown_{}
    , future_shutdown_{promise_shutdown_.get_future()}
    , command_queue_{}
    , message_queue_{}
    , deferred_queue_{}
    , log_{logger}
    , name_{name}
    , processing_thread_id_{}
{
    log_(name_)(" ")(__FUNCTION__).Flush();
    tdiag("Reactor::Reactor");
    queue_event_flag_.notify_one();
}

Reactor::~Reactor()
{
    tdiag("Reactor::~Reactor 1", processing_thread_id_);
    try {

        if (thread_ && !in_reactor_thread() && thread_->joinable()) {
            thread_->join();
        }
    } catch (const std::exception&) {
        // deliberately silent
    }
    tdiag("Reactor::~Reactor 2", processing_thread_id_);
}

int Reactor::reactor_loop()
{
    while (1) {
        if (active_) {
            std::unique_lock<std::mutex> lck(queue_event_mtx_);
            queue_event_flag_.wait(lck);
        }
        if (!active_) {
            {
                tdiag("leaving reactor", processing_thread_id_);
            }
            break;
        }

        auto cname = ThreadMonitor::default_name(get_class());
        std::string descr{name_};
        auto thread_handle = ThreadMonitor::add_current_thread(
            std::move(cname), std::move(descr));
        set_host_thread(thread_handle);
        processing_thread_id_ = std::this_thread::get_id();

        {
            tdiag("reactor initialization", processing_thread_id_);
        }

        while (active_) {
            {
                auto deferred_promises_handle = deferred_promises_.lock();
                if (!deferred_promises_handle->empty()) {
                    process_deferred();
                    deferred_promises_handle->front().set_value();
                    deferred_promises_handle->pop_front();
                }
            }

            while (active_) {
                auto c = dequeue_command();
                if (c) {
                    tdiag(typeid(this), "dequeue command");
                    time_it([&] { c->exec(); }, "XX COMMAND EXEC", 1200);
                }

                auto m = dequeue_message();
                if (!active_ || !m.has_value()) { break; }
                time_it(
                    [&] { handle(std::move(m.value())); },
                    "XX MESSAGE HANDLER",
                    11000);
                tadiag("Last job: ", last_job_str());
            }
            if (!active_) { break; }
            std::unique_lock<std::mutex> lck(queue_event_mtx_);
            time_it(
                [&] { queue_event_flag_.wait(lck); }, "XX NOTIFICATION", 1200);
        }
        break;
    }
    tdiag("leaving reactor", processing_thread_id_);
    promise_shutdown_.set_value();
    return 0;
}

}  // namespace opentxs
