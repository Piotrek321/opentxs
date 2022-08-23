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
#include "util/timed.hpp"

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
    virtual auto dispose() -> void = 0;
};

template <typename F>
class SynchronizableCommand : public Command
{
public:
    explicit SynchronizableCommand(F f)
        : Command{}
        , disarmed_{false}
        , task_{[this, f] {
            bool disarmed = disarmed_.exchange(false);
            if (!disarmed) { throw std::runtime_error("uncompleted task"); }
            return f();
        }}
    {
    }
    auto exec() -> void final
    {
        disarmed_ = true;
        task_();
    }
    auto dispose() -> void final { task_(); }
    auto get_future() { return task_.get_future(); }

private:
    using R = typename std::invoke_result<F>::type;
    std::atomic<bool> disarmed_;
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
        try {
            if (in_reactor_thread() || !active_) { return f(); }
            tdiag(typeid(this), "Reactor::synchronize");
            std::unique_ptr<SynchronizableCommand<Functor>> fc(
                new SynchronizableCommand(std::move(f)));
            auto ft = fc->get_future();
            {
                std::unique_lock<std::mutex> lk(mtx_queue_state);
                if (active_) {
                    command_queue_.push(std::move(fc));
                    notify();
                } else {
                    fc->exec();
                    fc.reset();
                }
            }
            return ft.get();
        } catch (const std::exception& e) {
            std::cerr << ThreadMonitor::get_name()
                      << " synchronize exception: " << e.what() << "\n";
            using R = typename std::invoke_result<Functor>::type;
            if constexpr (!std::is_same<R, void>::value) { return R{}; }
        }
    }

    // Handle a message in a subclass.
    virtual auto handle(
        network::zeromq::Message&& in,
        unsigned idx = 0) noexcept -> void = 0;

    // Add a message to reactor queue.
    auto post(network::zeromq::Message&& in, unsigned idx = 0) -> bool;

    // Schedule message execution
    auto post_at(
        network::zeromq::Message&& in,
        std::chrono::time_point<std::chrono::system_clock> t_at) -> bool;

    // Start the reactor thread.
    bool start();

    // Open reactor loop for exit.
    bool stop();

protected:
    // Check if the caller is the reactor loop.
    bool in_reactor_thread() const noexcept;

    // Say something may have been added to a queue.
    auto notify() -> void;

    // Submitting a message for deferred handling.
    auto defer(network::zeromq::Message&& in) -> void;

    // This is handling all deferred messages.
    auto flush_cache() -> void;

private:
    // for diagnostics only
    virtual auto last_job_str() const noexcept -> std::string = 0;

private:
    using Promises = std::queue<std::promise<void>>;
    std::atomic<bool> active_;
    Promises deferred_promises_;
    mutable std::mutex mtx_queue_state;
    mutable std::condition_variable cv_queue_state;
    std::unique_ptr<std::thread> thread_;

protected:
    using CommandQueue = std::queue<std::unique_ptr<Command>>;
    using MessageQueue = std::vector<std::queue<network::zeromq::Message>>;
    using SchedulerQueue = std::priority_queue<
        Timed<network::zeromq::Message, std::chrono::system_clock>,
        std::vector<Timed<network::zeromq::Message, std::chrono::system_clock>>,
        TimedBefore<network::zeromq::Message, std::chrono::system_clock>>;

    std::thread::id processing_thread_id();

private:
    CommandQueue command_queue_;
    MessageQueue message_queue_;
    MessageQueue deferred_queue_;
    SchedulerQueue scheduler_queue_;
    std::optional<std::chrono::time_point<std::chrono::system_clock>> deadline_;
    const Log& log_;
    std::string name_;

private:
    auto dequeue_message()
        -> std::optional<std::pair<network::zeromq::Message, unsigned>>;

    auto dequeue_scheduled_message()
        -> std::optional<std::pair<network::zeromq::Message, unsigned>>;

    auto dequeue_deferred() -> std::optional<network::zeromq::Message>;

    auto dequeue_command() noexcept -> std::unique_ptr<Command>;

    auto process_deferred() -> void;

    auto process_command(std::unique_ptr<Command>&&) -> void;
    auto process_message(network::zeromq::Message&&, int) -> void;
    auto process_scheduled(network::zeromq::Message&&) -> void;

protected:
    Reactor(const Log& logger, std::string name, unsigned qcount = 1) noexcept;
    ~Reactor() override;

    auto name() const noexcept { return name_; }

private:
    std::thread::id processing_thread_id_;

private:
    int reactor_loop() noexcept;
    int reactor_loop_raw();
    std::chrono::milliseconds time_to_deadline() const;
};
}  // namespace opentxs
