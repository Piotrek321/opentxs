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
bool ThreadMonitor::set_name(
    std::thread::native_handle_type h,
    std::string_view value)
{
    return 0 == pthread_setname_np(h, value.data());
}

// static
bool ThreadMonitor::set_name(std::string_view value)
{
    return 0 == pthread_setname_np(pthread_self(), value.data());
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
        auto ib = Class.find("<");
        auto ie = Class.find(">");
        if (ib != std::string::npos && ie != std::string::npos) {
            Class.erase(ib, ie - ib + 1);
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
    static const auto dict = std::map<std::string, std::string>{
        {"Account", "Acc"},      {"Accounts", "Accs"},
        {"Action", "Actn"},      {"Activity", "Acvt"},
        {"Actor", "Actr"},       {"Balance", "Blnc"},
        {"Bitcoin", "Bitc"},     {"Block", "Blck"},
        {"Blockchain", "Bchn"},  {"Data", "Data"},
        {"Client", "Clt"},       {"Deterministic", "Det"},
        {"Downloader", "Dldr"},  {"Event", "Evt"},
        {"Filter", "Flt"},       {"Imp", "Imp"},
        {"Header", "Hdr"},       {"Index", "Idx"},
        {"Indexer", "Idxr"},     {"Internal", "Int"},
        {"Jobs", "Jobs"},        {"Manager", "Mgr"},
        {"Miner", "Mnr"},        {"Network", "Nwk"},
        {"Notification", "Ntf"}, {"Oracle", "Orac"},
        {"Process", "Proc"},     {"Progress", "Prog"},
        {"Reactor", "Reac"},     {"Rescan", "Resc"},
        {"Scan", "Scan"},        {"Selection", "Sel"},
        {"Server", "Serv"},      {"Simple", "Smpl"},
        {"State", "Sta"},        {"Status", "Stt"},
        {"Statistics", "Scs"},   {"Summary", "Sum"},
        {"Thread", "Thr"},       {"Visitor", "Vis"},
        {"Wallet", "Wal"},       {"account", "ac"},
        {"accounts", "as"},      {"actor", "ar"},
        {"balance", "bl"},       {"balance", "bl"},
        {"base", "bs"},          {"block", "bk"},
        {"blockchain", "bn"},    {"blockoracle", "bo"},
        {"data", "dt"},          {"deterministic", "dm"},
        {"downloader", "dl"},    {"filter", "fl"},
        {"impl", "im"},          {"implementation", "im"},
        {"index", "ix"},         {"indexer", "ir"},
        {"internal", "in"},      {"jobs", "jb"},
        {"manager", "mr"},       {"network", "nk"},
        {"notification", "nf"},  {"oracle", "or"},
        {"process", "pr"},       {"progress", "pg"},
        {"reactor", "rr"},       {"rescan", "rs"},
        {"scan", "sc"},          {"server", "sv"},
        {"state", "st"},         {"wallet", "wt"}};

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
            auto& latest_word = members.back();
            latest_word += c;
        }
    }

    for (auto& w : members) {
        if (w.size() >= 4) {
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
    if (e) { std::cerr << "PTHREAD ERROR:" << std::strerror(e) << "\n"; }
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
    if (hit == handles_.end()) {
        std::ostringstream oss{};
        oss << std::hex << pthread_self();
        return ThreadHandle{oss.str(), std::string{}, 0};
    } else {
        return hit->second;
    }
}

std::string ThreadMonitor::p_get_name() { return p_get().name_; }

std::string ThreadMonitor::p_get_name(const ThreadHandle& handle)
{
    return handle.name_;
}

std::string ThreadMonitor::p_get_name(handle_type native)
{
    auto hit = handles_.find(native);
    if (hit != handles_.end()) { return hit->second.name_; }
    std::ostringstream oss{};
    oss << std::hex << pthread_self();
    return oss.str();
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

// static
std::int64_t Mytime::base = basecount();

Mytime::Mytime() {}
std::string Mytime::str()
{
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count() -
                base;
    return std::to_string(diff);
}

std::int64_t Mytime::basecount() noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
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

bool thread_local ThreadDisplay::enabled_for_thread_;
ThreadDisplay::Tdiag ThreadDisplay::enabled_ = ThreadDisplay::Tdiag::on;

void ThreadDisplay::enable(ThreadDisplay::Tdiag how) noexcept
{
    enabled_ = how;
}
void ThreadDisplay::thread_enable() noexcept { enabled_for_thread_ = true; }
void ThreadDisplay::thread_disable() noexcept { enabled_for_thread_ = false; }

void ThreadDisplay::ssdiag(
    std::string&& s1,
    std::string&& s2,
    std::string&& tag) const noexcept
{
    constexpr static const std::size_t ReservedDiagLength = 233;
    constexpr static const std::size_t TagWidth = 5;
    constexpr static const std::size_t TimeWidth = 8;
    constexpr static const std::size_t ThreadNameWidth = 17;
    constexpr static const std::size_t MessageWidth = 70;

    if (enabled_ == Tdiag::off ||
        (enabled_ == Tdiag::selective && !enabled_for_thread_)) {
        return;
    }
    std::unique_lock<std::mutex> l(diagmtx);
    std::string C = get_class();

    thread_local static std::string line(ReservedDiagLength, ' ');
    constexpr static const std::size_t idx0 = 0;

    line.resize(idx0);
    line += tag;
    constexpr static const std::size_t idx1 = idx0 + TagWidth;

    line.resize(idx1, ' ');
    line += mytime();
    constexpr static const std::size_t idx2 = idx1 + TimeWidth;

    line.resize(idx2, ' ');
    line += ThreadMonitor::get_name();
    constexpr static const std::size_t idx3 = idx2 + ThreadNameWidth;

    line.resize(idx3, ' ');
    line += thread_handle_.has_value()
                ? ThreadMonitor::get_name(thread_handle_.value())
                : std::string(ThreadNameWidth - 2, '-');
    constexpr static const std::size_t idx4 = idx3 + ThreadNameWidth;

    line.resize(idx4, ' ');
    line += C;
    constexpr static const std::size_t idx5 = idx4 + MessageWidth;

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

void ThreadDisplay::tdiag(std::string&& s1) const noexcept
{
    if (is_enabled()) ssdiag(std::move(s1));
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

bool ThreadDisplay::is_enabled() const noexcept
{
    return enabled_ == Tdiag::on ||
           (enabled_ == Tdiag::selective && enabled_for_thread_);
}

ThreadDisplay::~ThreadDisplay() {}

MessageMarker::MessageMarker(network::zeromq::Message& m, bool remove)
    : tag_{'\0'}
    , check_{}
    , name_{'\0'}
{
    const auto body = m.Body();
    const auto level = body.size();
    if (level == 0) { return; }
    try {
        *this = body.at(level - 1).as<MessageMarker>();
        if (remove) { m.PopFrame(); }
    } catch (...) {
    }
}
MessageMarker::MessageMarker(std::string_view tag, std::string threadname)
    : tag_{'\0'}
    , check_{Marker}
    , name_{'\0'}
{
    if (tag.size()) {
        std::memcpy(&tag_[0], tag.data(), std::min(tag.size(), sizeof(tag_)));
    }
    std::memcpy(
        &name_[0],
        threadname.data(),
        std::min(threadname.size() + 1, sizeof(name_)));
}
std::size_t MessageMarker::mark(network::zeromq::Message& m)
{
    m.AddFrame(*this);
    return m.Body().size();
}
MessageMarker::operator bool() const noexcept { return check_ == Marker; }
std::ostream& operator<<(std::ostream& os, const MessageMarker& m)
{
    if (m.check_ != MessageMarker::Marker) {
        return os << "???";
    } else {
        return os << std::hex << m.tag_ << std::dec << ' ' << m.name_;
    }
}
std::string to_string(const MessageMarker& mm)
{
    std::ostringstream oss{};
    oss << mm;
    return oss.str();
}

#endif  // defined(__linux) && defined(TDIAG)

}  // namespace opentxs
