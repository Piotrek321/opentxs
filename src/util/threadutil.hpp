#ifndef util_threadutil_hpp_
#define util_threadutil_hpp_

#include <boost/core/demangle.hpp>
#include <pthread.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
using namespace std::literals::chrono_literals;

//#define TDIAG

#if defined(__linux) && defined(TDIAG)

using handle_type = std::thread::native_handle_type;
struct ThreadHandle {
    ThreadHandle();
    ThreadHandle(
        std::string&& name,
        std::string&& description,
        handle_type handle);
    std::string name_;
    std::string description_;
    handle_type native_;
};

class ThreadMonitor
{
public:
    // Register the thread
    static ThreadHandle add_current_thread(
        std::string&& name,
        std::string&& description);
    static std::string get_name();
    static std::string get_name(const ThreadHandle& handle);
    static std::string get_name(handle_type native);
    static bool set_name(
        std::thread::native_handle_type h,
        std::string_view value);
    static bool set_name(std::string_view value);
    // Arbitrary maximum thread name length...
    static constexpr std::size_t max_name_length();
    static std::string default_name(std::string&& Class);
    enum class State { Unknown, Idle, Waiting, Blocked, Deadlocked };
    struct ThreadInfo {
        explicit ThreadInfo(
            handle_type = {},
            State state = State{},
            std::string blocking_resource_str = {},
            std::string blocking_comment_str = {});
        handle_type native_;
        State state_;
        std::string blocking_resource_str_;
        std::string blocking_comment_str_;
    };
    static std::vector<ThreadInfo> get_snapshot();

private:
    static ThreadMonitor* instance();
    ThreadMonitor();
    ~ThreadMonitor();
    ThreadHandle p_add_current_thread(
        std::string&& name,
        std::string&& description);
    ThreadHandle p_get();
    std::string p_get_name();
    std::string p_get_name(const ThreadHandle& handle);
    std::string p_get_name(handle_type native);
    std::vector<ThreadInfo> p_get_snapshot();
    ThreadInfo get_info(const ThreadHandle& handle);

private:
    static std::mutex mutex_;
    static ThreadMonitor* instance_;
    std::map<handle_type, ThreadHandle> handles_;
    std::set<std::string> names_;
    std::map<handle_type, ThreadInfo> thread_info_;
};

struct Mytime {
    Mytime();
    static std::int64_t basecount() noexcept;
    static std::int64_t base;
    std::string str();
};

inline std::string mytime()
{
    Mytime m{};
    return m.str();
}

class ThreadDisplay
{
public:
    ThreadDisplay();
    void set_host_thread(const ThreadHandle& value);
    static void show(const ThreadMonitor::ThreadInfo& thr, std::ostream& os);
    static void show_all(std::ostream& os);

    enum class Tdiag { on, selective, off };
    static void enable(Tdiag) noexcept;
    static void thread_enable() noexcept;
    static void thread_disable() noexcept;

protected:
    void tdiag(std::string&& s1) const noexcept;
    template <typename S1, typename S2>
    void tdiag(S1&& s1, S2&& s2, std::string&& tag = "####") const noexcept
    {
        if (!is_enabled()) { return; }
        if constexpr (std::is_same<
                          typename std::decay<S1>::type,
                          std::type_info>::value) {
            tsdiag(std::move(s1), std::move(s2), std::move(tag));
        } else {
            if constexpr (std::is_same<
                              typename std::decay<S2>::type,
                              std::string>::value) {
                ssdiag(std::move(s1), std::move(s2), std::move(tag));
            } else {
                std::ostringstream oss;
                oss << s2;
                ssdiag(std::move(s1), oss.str(), std::move(tag));
            }
        }
    }
    void tadiag(
        std::string&& s1,
        std::string&& s2 = "",
        std::string&& tag = "##  ") const noexcept;
    void tadiag(
        const std::type_info& type,
        std::string&& s2 = "",
        std::string&& tag = "##  ") const noexcept;
    std::string get_class() const noexcept;
    template <typename Functor>
    bool time_it(
        Functor f,
        std::string tag = "",
        std::chrono::milliseconds max_ms = 5000ms)
    {
        auto tstart = std::chrono::system_clock::now();
        f();
        auto tfinish = std::chrono::system_clock::now();
        if (auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                tfinish - tstart);
            ms > max_ms) {
            diag_alert_ = true;
            tdiag(std::move(tag), std::to_string(ms.count()) + " ms");
            return false;
        }
        return true;
    }

    virtual ~ThreadDisplay();

private:
    bool is_enabled() const noexcept;
    void ssdiag(
        std::string&& s1,
        std::string&& s2 = "",
        std::string&& tag = "####") const noexcept;
    void tsdiag(
        const std::type_info& type,
        std::string&& s2 = "",
        std::string&& tag = "####") const noexcept;

private:
    static std::mutex diagmtx;
    std::optional<ThreadHandle> thread_handle_;
    mutable std::atomic<bool> diag_alert_;
    static thread_local bool enabled_for_thread_;
    static Tdiag enabled_;
};

class MessageMarker
{
public:
    explicit MessageMarker(network::zeromq::Message& m, bool remove = true);
    explicit MessageMarker(
        std::string_view tag = {},
        std::string threadname = ThreadMonitor::get_name());
    std::size_t mark(network::zeromq::Message& m);
    operator bool() const noexcept;

private:
    constexpr static std::size_t Text_Size = 16;
    constexpr static std::uint64_t Marker = 0xDEAD00FF;
    friend std::ostream& operator<<(std::ostream&, const MessageMarker&);
    char tag_[Text_Size];
    std::uint32_t check_;
    char name_[Text_Size];
};

std::ostream& operator<<(std::ostream& os, const MessageMarker& m);

std::string to_string(const MessageMarker&);

#else  // defined(__linux) && defined(TDIAG)

using handle_type = std::thread::native_handle_type;
struct ThreadHandle {
    ThreadHandle() {}
    ThreadHandle(std::string&&, std::string&&, handle_type) {}
};

class ThreadMonitor
{
public:
    struct ThreadInfo {
    };
    static ThreadHandle add_current_thread(std::string&&, std::string&&)
    {
        return {};
    }
    static std::string get_name() { return {}; }
    static std::string get_name(const ThreadHandle&) { return {}; }
    static std::string get_name(handle_type native) { return {}; }
    static std::string default_name() { return {}; }
    static bool set_name(std::thread::native_handle_type, std::string_view)
    {
        return true;
    }
    static bool set_name(std::string_view) { return true; }
    // Arbitrary maximum thread name length...
    static constexpr std::size_t max_name_length() noexcept { return 0; }
    static std::string default_name(std::string&& Class) noexcept { return {}; }
    enum class State { Unknown, Idle, Waiting, Blocked, Deadlocked };
    static std::vector<ThreadInfo> get_snapshot() { return {}; }
};

class ThreadDisplay
{
public:
    ThreadDisplay() = default;
    virtual ~ThreadDisplay() {}
    void set_host_thread(const ThreadHandle&) {}
    static void show(const ThreadMonitor::ThreadInfo& thr, std::ostream& os) {}
    static void show_all(std::ostream& os) {}
    enum class Tdiag { on, selective, off };
    static void enable(Tdiag) noexcept {}
    static void thread_enable() noexcept {}
    static void thread_disable() noexcept {}

protected:
    void tdiag(std::string&&) const noexcept {}
    template <typename S1, typename S2>
    void tdiag(S1&&, S2&&, std::string&& = "") const noexcept
    {
    }
    void tadiag(std::string&&, std::string&& = "", std::string&& = "")
        const noexcept
    {
    }
    void tadiag(const std::type_info&, std::string&& = "", std::string&& = "")
        const noexcept
    {
    }

    std::string get_class() const noexcept { return {}; }
    template <typename Functor>
    static void time_it(
        Functor f,
        std::string = {},
        std::chrono::milliseconds = {})
    {
        f();
    }
};

class MessageMarker
{
public:
    explicit MessageMarker(network::zeromq::Message&, bool = true) {}
    explicit MessageMarker(
        std::string_view = {},
        std::string = ThreadMonitor::get_name())
    {
    }
    std::size_t mark(network::zeromq::Message&) { return 0; }
    operator bool() const noexcept { return false; }
};

inline std::ostream& operator<<(std::ostream& os, const MessageMarker&)
{
    return os;
}

inline std::string to_string(const MessageMarker&) { return {}; }

#endif  // defined(__linux) && defined(TDIAG)

}  // namespace opentxs

#endif  // util_threadutil_hpp_
