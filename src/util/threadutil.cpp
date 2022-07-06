#include <boost/core/demangle.hpp>
#include <pthread.h>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

#include "util/threadutil.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
#if defined(__linux) && defined(TDIAG)

ThreadHandle::ThreadHandle()
    : name_{}
    , description_{}
    , native_{}
{
}
ThreadHandle::ThreadHandle(
    std::string&& name,
    std::string&& description,
    handle_type handle)
    : name_{name}
    , description_{description}
    , native_{handle}
{
}

// static
ThreadHandle ThreadMonitor::add_current_thread(
    std::string&& name,
    std::string&& description)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return instance()->p_add_current_thread(
        std::move(name), std::move(description));
}

ThreadMonitor::ThreadInfo::ThreadInfo(
    handle_type native,
    State state,
    std::string blocking_resource_str,
    std::string blocking_comment_str)
    : native_{native}
    , state_{state}
    , blocking_resource_str_{blocking_resource_str}
    , blocking_comment_str_{blocking_comment_str}
{
}

// static
std::string ThreadMonitor::get_name()
{
    std::unique_lock<std::mutex> lock(mutex_);
    return instance()->p_get_name();
}

std::string ThreadMonitor::get_name(const ThreadHandle& handle)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return instance()->p_get_name(handle);
}

std::string ThreadMonitor::get_name(handle_type native)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return instance()->p_get_name(native);
}

// static
constexpr std::size_t ThreadMonitor::max_name_length()
{
    // Arbitrary limit for a displayable name
    return 30;
}

std::string ThreadMonitor::default_name(std::string&& Class)
{
    constexpr const char* opentxs_tag = "opentxs::";
    constexpr const auto tag_length = 9;
    while (true) {
        auto i = Class.find(opentxs_tag);
        if (i == std::string::npos) { break; }
        Class.erase(i, tag_length);
    }
    while (true) {
        auto p1 = Class.find_first_of("::");
        if (p1 == std::string::npos || p1 == Class.size() - 2) { break; }
        auto p2 = Class.find_first_of("::", p1 + 2);
        if (p2 == std::string::npos) { break; }
        Class.erase(0, p1 + 2);
    }
    if (Class.size() > max_name_length()) {
        if (auto i = Class.find("<"); i != std::string::npos) {
            Class.erase(max_name_length(), std::string::npos);
        } else {
            Class.erase(0, Class.size() - max_name_length());
        }
    }
    if (Class.find_first_of("::") == 0) { Class.erase(0, 2); }
    return std::move(Class);
}

// static
std::vector<ThreadMonitor::ThreadInfo> ThreadMonitor::get_snapshot()
{
    std::unique_lock<std::mutex> lock(mutex_);
    return instance()->p_get_snapshot();
}

// static
ThreadMonitor* ThreadMonitor::instance()
{
    if (!instance_) { instance_ = new ThreadMonitor; }
    return instance_;
}

ThreadMonitor::~ThreadMonitor() {}

std::string abbreviate(std::string&& full);

std::string abbreviate(std::string&& full)
{
    static auto dict = std::map<std::string, std::string>{
        {"Account", "Acct"},
        {"Accounts", "Acts"},
        {"Actor", "Actr"},
        {"Balance", "Blnc"},
        {"Bitcoin", "Btco"},
        {"Block", "Blck"},
        {"Blockchain", "Blkc"},
        {"Data", "Data"},
        {"Deterministic", "Dtrm"},
        {"Downloader", "Dldr"},
        {"Filter", "Fltr"},
        {"Imp", "Imp"},
        {"Index", "Indx"},
        {"Indexer", "Idxr"},
        {"Index", "Indx"},
        {"Internal", "Intr"},
        {"Jobs", "Jobs"},
        {"Manager", "Mngr"},
        {"Network", "Ntwk"},
        {"Oracle", "Orcl"},
        {"Process", "Proc"},
        {"Progress", "Prog"},
        {"Reactor", "Reac"},
        {"Rescan", "Resc"},
        {"Scan", "Scan"},
        {"Server", "Srv"},
        {"State", "Stte"},
        {"Wallet", "Wlt"},
        {"account", "acct"},
        {"accounts", "acts"},
        {"actor", "actr"},
        {"balance", "blnc"},
        {"bitcoin", "btco"},
        {"block", "blck"},
        {"blockchain", "blkc"},
        {"blockoracle", "blor"},
        {"data", "data"},
        {"deterministic", "dtrm"},
        {"downloader", "dldr"},
        {"filter", "fltr"},
        {"imp", "imp"},
        {"index", "indx"},
        {"indexer", "idxr"},
        {"index", "indx"},
        {"internal", "intr"},
        {"jobs", "jobs"},
        {"manager", "mngr"},
        {"network", "ntwk"},
        {"oracle", "orcl"},
        {"process", "proc"},
        {"progress", "prog"},
        {"reactor", "reac"},
        {"rescan", "resc"},
        {"scan", "scan"},
        {"server", "srv"},
        {"state", "stte"},
        {"wallet", "wlt"}};

    while (true) {
        auto idouble_colon = full.find("::");
        if (idouble_colon == std::string::npos) break;
        full.replace(idouble_colon, 2, "_");
    }

    std::vector<std::string> members;
    members.push_back({});
    for (auto c : full) {
        auto& word = members.back();
        if (islower(c)) { word += c; }
        if (c == '_') {
            members.push_back({});
            continue;
        }
        if (isupper(c)) {
            members.push_back({});
            auto& word = members.back();
            word += c;
        }
    }

    for (auto& w : members) {
        if (w.size() > 4) {
            if (auto i = dict.find(w); i != dict.end()) {
                w = i->second;
            } else {
                w.resize(4);
            }
        }
    }

    std::string result;
    while (!members.empty()) {
        result = members.back() + result;
        members.pop_back();
        if (result.size() > 13) {
            result.resize(13);
            break;
        }
    }
    return result;
}

ThreadHandle ThreadMonitor::p_add_current_thread(
    std::string&& name,
    std::string&& description)
{
    constexpr const unsigned max_pthread_name_len = 15;
    std::string thr_name = abbreviate(std::move(name));
    for (auto idx = 0; idx < 100; ++idx) {
        std::string scratch_name = thr_name;
        if (idx) { scratch_name += std::to_string(idx); }

        if (scratch_name.size() > max_pthread_name_len) {
            scratch_name.erase(0, scratch_name.size() - max_pthread_name_len);
        }

        if (names_.find(scratch_name) == names_.end()) {
            thr_name = std::move(scratch_name);
            break;
        }
    }
    int e = pthread_setname_np(pthread_self(), thr_name.data());
    if (e) { std::cerr << "!!!!!!!!!!!!!!!!!!" << std::strerror(e) << "\n"; }
    ThreadHandle th{
        std::move(thr_name),
        std::move(description),
        static_cast<handle_type>(pthread_self())};
    names_.insert(th.name_);
    thread_info_[th.native_] =
        ThreadInfo{pthread_self(), State::Unknown, {}, {}};
    return handles_[th.native_] = std::move(th);
}

ThreadHandle ThreadMonitor::p_get()
{
    auto hit = handles_.find(static_cast<handle_type>(pthread_self()));
    return hit == handles_.end() ? ThreadHandle{std::string{}, std::string{}, 0}
                                 : hit->second;
}

std::string ThreadMonitor::p_get_name() { return p_get().name_; }

std::string ThreadMonitor::p_get_name(const ThreadHandle& handle)
{
    return handle.name_;
}

std::string ThreadMonitor::p_get_name(handle_type native)
{
    return handles_[native].name_;
}

ThreadMonitor::ThreadInfo ThreadMonitor::get_info(const ThreadHandle& handle)
{
    return thread_info_[handle.native_];
}

std::vector<ThreadMonitor::ThreadInfo> ThreadMonitor::p_get_snapshot()
{
    std::vector<ThreadInfo> result{};
    result.reserve(thread_info_.size());
    for (auto& p : thread_info_) { result.push_back(p.second); }
    return result;
}

ThreadMonitor::ThreadMonitor()
    : handles_{}
    , names_{}
    , thread_info_{}
{
}

std::mutex ThreadMonitor::mutex_ = {};
ThreadMonitor* ThreadMonitor::instance_ = nullptr;

Mytime::Mytime() {}
std::string Mytime::str()
{
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count() -
                base;
    return std::to_string(diff);
}

// static
std::mutex ThreadDisplay::diagmtx{};

ThreadDisplay::ThreadDisplay()
    : thread_handle_{}
    , diag_alert_{}
{
}
void ThreadDisplay::set_host_thread(const ThreadHandle& value)
{
    std::unique_lock<std::mutex> l(diagmtx);
    thread_handle_ = value;
}

void ThreadDisplay::show(const ThreadMonitor::ThreadInfo& thr, std::ostream& os)
{
    std::unique_lock<std::mutex> l(diagmtx);
    std::cerr << "# " << std::left
              << std::setw(ThreadMonitor::max_name_length() + 2)
              << ThreadMonitor::get_name(thr.native_);
    std::cerr << std::left << std::setw(ThreadMonitor::max_name_length() + 2);
    std::cerr << "\n";
}

void ThreadDisplay::show_all(std::ostream& os)
{
    std::cerr << " THREADS\n";

    auto all = ThreadMonitor::get_snapshot();
    for (auto& t : all) { show(t, os); }
    std::cerr << "------\n";
}

void ThreadDisplay::ssdiag(
    std::string&& s1,
    std::string&& s2,
    std::string&& tag) const noexcept
{
    std::unique_lock<std::mutex> l(diagmtx);
    std::string C = get_class();

    constexpr std::size_t ReservedDiagLength = 232;
    thread_local static std::string line(ReservedDiagLength, ' ');
    constexpr std::size_t idx0 = 0;
    line.resize(idx0);
    line += tag;
    constexpr std::size_t idx1 = idx0 + 5;
    line.resize(idx1, ' ');
    line += mytime();
    constexpr std::size_t idx2 = idx1 + 7;
    line.resize(idx2, ' ');
    line += ThreadMonitor::get_name();
    constexpr std::size_t idx3 = idx2 + 17;
    line.resize(idx3, ' ');
    line += thread_handle_.has_value()
                ? ThreadMonitor::get_name(thread_handle_.value())
                : std::string("----------");
    constexpr std::size_t idx4 = idx3 + 17;
    line.resize(idx4, ' ');
    line += C;
    constexpr std::size_t idx5 = idx4 + 70;
    line.resize(idx5, ' ');
    line += s1;
    line += " ";
    if (s2.size()) { line += s2; }
    line += "\n";
    static_assert(idx5 + 64 < ReservedDiagLength);
    std::cerr << line;
}

void ThreadDisplay::tsdiag(
    const std::type_info& type,
    std::string&& s2,
    std::string&& tag) const noexcept
{
    thread_local static std::string s1;
    {
        std::unique_lock<std::mutex> l(diagmtx);
        s1 = boost::core::demangle(type.name());
    }
    ssdiag(std::move(s1), std::move(s2), std::move(tag));
}

void ThreadDisplay::tadiag(
    std::string&& s1,
    std::string&& s2,
    std::string&& tag) const noexcept
{
    if (diag_alert_.exchange(false)) {
        tdiag(std::move(s1), std::move(s2), std::move(tag));
    }
}

void ThreadDisplay::tadiag(
    const std::type_info& type,
    std::string&& s2,
    std::string&& tag) const noexcept
{
    if (diag_alert_.exchange(false)) {
        tadiag(type, std::move(s2), std::move(tag));
    }
}

std::string ThreadDisplay::get_class() const noexcept
{
    std::string s = typeid(*this).name();
    std::string Class = boost::core::demangle(s.data());
    return Class;
}

ThreadDisplay::~ThreadDisplay() {}

#endif  // defined(__linux) && defined(TDIAG)

}  // namespace opentxs
