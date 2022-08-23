// IWYU pragma: no_include "opentxs/network/zeromq/socket/SocketType.hpp"

#include "util/Reactor.hpp"

namespace opentxs
{

std::thread::id Reactor::processing_thread_id()
{
    return processing_thread_id_;
}

auto Reactor::post(network::zeromq::Message&& in, unsigned idx) -> bool
{
    if (active_) {
        auto& mq = message_queue_.at(idx);
        std::unique_lock<std::mutex> lk(mtx_queue_state);
        mq.push(std::move(in));
        notify();
        return true;
    }
    return false;
}

auto Reactor::post_at(
    network::zeromq::Message&& in,
    std::chrono::time_point<std::chrono::system_clock> t_at) -> bool
{
    if (!active_) { return false; }
    std::unique_lock<std::mutex> lck(mtx_queue_state);
    if (!scheduler_queue_.empty()) { return false; }
    if (!active_) { return false; }
    scheduler_queue_.push(
        Timed<network::zeromq::Message, std::chrono::system_clock>{
            std::move(in), t_at});
    if (!deadline_.has_value() || t_at < deadline_.value()) deadline_ = t_at;
    notify();
    return true;
}

auto Reactor::defer(network::zeromq::Message&& in) -> void
{
    assert(thread_);
    auto& mq = deferred_queue_[0];
    std::unique_lock<std::mutex> lk(mtx_queue_state);
    mq.push(in);
    // Not notifying as deferred execution will be triggered explicitly.
}

auto Reactor::dequeue_message()
    -> std::optional<std::pair<network::zeromq::Message, unsigned>>
{
    for (auto idx = 0u; idx < message_queue_.size(); ++idx) {
        auto& mq = message_queue_[idx];
        std::unique_lock<std::mutex> lck(mtx_queue_state);
        if (!mq.empty()) {
            auto msg = std::make_pair(std::move(mq.front()), idx);
            mq.pop();
            return msg;
        }
    }
    return {std::nullopt};
}

auto Reactor::dequeue_scheduled_message()
    -> std::optional<std::pair<network::zeromq::Message, unsigned>>
{
    std::unique_lock<std::mutex> lck(mtx_queue_state);
    if (scheduler_queue_.empty()) { return {}; }

    auto& mt = scheduler_queue_.top();
    auto t_now = std::chrono::system_clock::now();
    auto t_sch = mt.t_scheduled_;

    if (t_now >= t_sch) {
        auto msg_int_pair = std::make_pair(std::move(mt.m_), 0);
        scheduler_queue_.pop();
        while (!scheduler_queue_.empty()) {
            deadline_ = scheduler_queue_.top().t_scheduled_;
            if (deadline_.value() != t_sch) { break; }
            scheduler_queue_.pop();
        }
        if (scheduler_queue_.empty()) { deadline_.reset(); }
        return msg_int_pair;
    }
    return {};
}

auto Reactor::dequeue_deferred() -> std::optional<network::zeromq::Message>
{
    std::optional<network::zeromq::Message> msg{};
    auto& mq = deferred_queue_[0];
    std::unique_lock<std::mutex> lck(mtx_queue_state);
    if (!mq.empty()) {
        msg = std::move(mq.front());
        mq.pop();
    }
    return msg;
}

auto Reactor::dequeue_command() noexcept -> std::unique_ptr<Command>
{
    std::unique_lock<std::mutex> lck(mtx_queue_state);
    if (!command_queue_.empty()) {
        auto c = std::move(command_queue_.front());
        command_queue_.pop();
        return c;
    }
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
            500ms);
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
            std::unique_lock<std::mutex> lck(mtx_queue_state);
            deferred_promises_.push(std::move(p));
            notify();
        }
        f.get();
    }
}

bool Reactor::start()
{
    tdiag("starting");
    if (active_.exchange(true)) return false;

    thread_.reset(new std::thread(&Reactor::reactor_loop, this));

    return true;
}

bool Reactor::stop()
{
    auto was_active = active_.exchange(false);
    if (was_active) {
        while (true) {
            if (auto c = dequeue_command(); c) {
                c->exec();
            } else {
                break;
            }
        }

        // Abort all messages

        for (auto& mq : message_queue_) {
            while (!mq.empty()) { mq.pop(); }
        }
        for (auto& mq : deferred_queue_) {
            while (!mq.empty()) { mq.pop(); }
        }
        while (!scheduler_queue_.empty()) { scheduler_queue_.pop(); }

        // Avoid breaking promises
        while (!deferred_promises_.empty()) {
            deferred_promises_.front().set_value();
            deferred_promises_.pop();
        }

        notify();
    }
    bool can_join = [this]() {
        return thread_ && !in_reactor_thread() && thread_->joinable();
    }();
    notify();
    if (can_join) {
        auto target = ThreadMonitor::get_name(thread_->native_handle());
        auto me = ThreadMonitor::get_name();
        std::cerr << me << " about to join " << target << "\n";
        thread_->join();
        std::cerr << me << " joined " << target << "\n";
        std::unique_lock<std::mutex> lck(mtx_queue_state);
        thread_.reset();
    }
    return was_active;
}

auto Reactor::notify() -> void { cv_queue_state.notify_one(); }

bool Reactor::in_reactor_thread() const noexcept
{
    return thread_ && thread_->get_id() == std::this_thread::get_id();
}

auto Reactor::process_command(std::unique_ptr<Command>&& in) -> void
{
    tdiag(typeid(this), "dequeue command");
    time_it(
        [&] {
            try {
                in->exec();
            } catch (const std::exception& e) {
                tdiag("Failed command: ", e.what());
            }
        },
        "XX COMMAND EXEC",
        1200ms);
}

auto Reactor::process_message(network::zeromq::Message&& in, int idx) -> void
{
    time_it([&]() { handle(std::move(in), idx); }, "XX MESSAGE HANDLER", 800ms);
    tadiag("Last job: ", last_job_str());
}

auto Reactor::process_scheduled(network::zeromq::Message&& in) -> void
{
    time_it(
        // Present implementation only allows a single schedueld queue.
        [&]() { handle(std::move(in), 0); },
        "XX SCHEDULED MESSAGE HANDLER",
        80ms);
    tadiag("Last job: ", last_job_str());
}

Reactor::Reactor(const Log& logger, std::string name, unsigned qcount) noexcept
    : ThreadDisplay()
    , active_{}
    , deferred_promises_{}
    , mtx_queue_state{}
    , cv_queue_state{}
    , thread_{}
    , command_queue_{}
    , message_queue_(qcount)
    , deferred_queue_(1)
    , scheduler_queue_{}
    , deadline_{}
    , log_{logger}
    , name_{std::move(name)}
    , processing_thread_id_{}
{
    log_(name_)(" ")(__FUNCTION__).Flush();
    tdiag("Reactor::Reactor");
    notify();
}

Reactor::~Reactor()
{
    tdiag("Reactor::~Reactor 1", processing_thread_id_);
    try {
        stop();
    } catch (const std::exception&) {
        // deliberately silent
    }
    tdiag("Reactor::~Reactor 2", processing_thread_id_);
}

int Reactor::reactor_loop() noexcept
{
    try {
        reactor_loop_raw();
    } catch (const std::exception& e) {
        tdiag("EXC:", e.what());
        exit(0);
    }
    return 0;
}

std::chrono::milliseconds Reactor::time_to_deadline() const
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline_.value() - now);
}

int Reactor::reactor_loop_raw()
{
    using namespace std::literals;

    auto cname = ThreadMonitor::default_name(get_class());
    std::string descr{name_};
    auto thread_handle =
        ThreadMonitor::add_current_thread(std::move(cname), std::move(descr));
    set_host_thread(thread_handle);
    processing_thread_id_ = std::this_thread::get_id();

    {
        tdiag("reactor initialization complete", processing_thread_id_);
    }

    while (active_) {
        {
            std::unique_lock<std::mutex> lck(mtx_queue_state);
            if (!deferred_promises_.empty()) {
                {
                    process_deferred();
                }
                deferred_promises_.front().set_value();
                deferred_promises_.pop();
            }
        }

        auto cmd = dequeue_command();
        if (!active_) {
            break;
        } else if (cmd) {
            process_command(std::move(cmd));
            continue;
        }

        auto sch_msg = dequeue_scheduled_message();
        if (!active_) {
            break;
        } else if (sch_msg.has_value()) {
            process_scheduled(std::move(sch_msg.value().first));
            continue;
        }

        auto msg_idx = dequeue_message();
        if (!active_) {
            break;
        } else if (msg_idx.has_value()) {
            auto& [msg, idx] = msg_idx.value();
            process_message(std::move(msg), idx);
            continue;
        }

        if (!active_) { break; }
        time_it(
            [&] {
                // Note the mutex protects all queues and the deadline.
                std::unique_lock<std::mutex> lck(mtx_queue_state);
                if (!deadline_.has_value()) {
                    cv_queue_state.wait(lck);
                } else if (time_to_deadline() > 1ms) {
                    cv_queue_state.wait_until(lck, deadline_.value());
                } else {
                    cv_queue_state.wait_for(lck, 2ms);
                }
            },
            "XX NOTIFICATION",
            20000ms);
    }
    while (!deferred_promises_.empty()) {
        // TODO check if setting exception would be useful
        deferred_promises_.front().set_value();
        deferred_promises_.pop();
    }
    tdiag("leaving reactor ", processing_thread_id_);
    return 0;
}

}  // namespace opentxs
