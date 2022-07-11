// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"          // IWYU pragma: associated
#include "1_Internal.hpp"        // IWYU pragma: associated
#include "opentxs/util/Log.hpp"  // IWYU pragma: associated

#include <boost/multiprecision/cpp_dec_float.hpp>  // IWYU pragma: keep
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/stacktrace.hpp>
#include <boost/system/error_code.hpp>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

#include "internal/core/Amount.hpp"
#include "internal/otx/common/StringXML.hpp"
#include "internal/otx/common/util/Common.hpp"
#include "internal/util/Log.hpp"
#include "opentxs/OT.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/blockchain/block/Outpoint.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/Armored.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/core/UnitType.hpp"
#include "opentxs/core/display/Definition.hpp"
#include "opentxs/core/display/Scale.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Notary.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ZeroMQ.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "util/Log.hpp"

namespace zmq = opentxs::network::zeromq;

namespace opentxs::internal
{
auto Log::Endpoint() noexcept -> const char*
{
    static const auto output = zmq::MakeDeterministicInproc("logsink", -1, 1);

    return output.c_str();
}

auto Log::SetVerbosity(const int level) noexcept -> void
{
    static auto& logger = opentxs::Log::Imp::logger_;
    logger.verbosity_ = level;
}

auto Log::Shutdown() noexcept -> void
{
    static auto& logger = opentxs::Log::Imp::logger_;
    logger.Stop();
}

auto Log::Start() noexcept -> void
{
    static auto& logger = opentxs::Log::Imp::logger_;
    logger.Start();
}
}  // namespace opentxs::internal

namespace opentxs
{
auto Log::Imp::Logger::get() const noexcept -> Ticket
{
    auto handle = gate_.lock_shared();

    assert(handle->has_value());

    return handle->value().get();
}

auto Log::Imp::Logger::CreateBuffer() noexcept
    -> std::pair<int, SourceMap::iterator>
{
    auto handle = map_.lock();
    auto& map = *handle;
    const auto index = ++logger_.index_;
    auto [it, added] = map.try_emplace(
        index,
        [] {
            using Direction = opentxs::network::zeromq::socket::Direction;
            auto out = Context().ZMQ().PushSocket(Direction::Connect);
            const auto rc = out->Start(internal::Log::Endpoint());

            assert(rc);

            return out;
        }(),
        std::stringstream{});

    assert(added);

    return std::make_pair(index, it);
}

auto Log::Imp::Logger::DestroyBuffer(int i) noexcept -> void
{
    map_.lock()->erase(i);
}

auto Log::Imp::Logger::Start() noexcept -> void
{
    auto handle = gate_.lock();
    handle->reset();
    handle->emplace();
    ++session_;
}

auto Log::Imp::Logger::Stop() noexcept -> void
{
    if (auto handle = gate_.lock(); handle->has_value()) {
        handle->value().shutdown();
        handle->reset();
    }

    map_.lock()->clear();
}
}  // namespace opentxs

namespace opentxs
{
Log::Imp::Logger Log::Imp::logger_{};

Log::Imp::Imp(const int logLevel, opentxs::Log& parent) noexcept
    : level_(logLevel)
    , parent_(parent)
{
}

auto Log::Imp::Abort() const noexcept -> void
{
    send(LogAction::terminate, Console::err);
    wait_for_terminate();
}

auto Log::Imp::active() const noexcept -> bool
{
    return logger_.verbosity_.load() >= level_;
}

auto Log::Imp::Assert(
    const char* file,
    const std::size_t line,
    const char* message) const noexcept -> void
{
    if (auto ticket = logger_.get(); false == ticket) {
        auto id = UnallocatedCString{};
        auto& [socket, buffer] = get_buffer(id);
        buffer = std::stringstream{};
        buffer << "OT ASSERT";

        if (nullptr != file) { buffer << " in " << file << " line " << line; }

        if (nullptr != message) { buffer << ": " << message; }

        buffer << "\n" << boost::stacktrace::stacktrace();
    }

    Abort();
}

auto Log::Imp::Flush() const noexcept -> void
{
    const auto console = [&] {
        switch (level_) {
            case 0: {

                return Console::out;
            }
            default: {

                return Console::err;
            }
        }
    }();
    const auto action = [&] {
        switch (level_) {
            case -2: {

                return LogAction::terminate;
            }
            default: {

                return LogAction::flush;
            }
        }
    }();
    send(action, console);
}

auto Log::Imp::get_buffer(UnallocatedCString& out) noexcept -> Logger::Source&
{
    struct Buffer {
        const std::thread::id id_;
        const UnallocatedCString text_;
        int session_;
        std::pair<int, Logger::SourceMap::iterator> source_;

        auto Refresh() noexcept -> void
        {
            if (auto session = logger_.session_.load(); session != session_) {
                source_ = logger_.CreateBuffer();
                session_ = session;
            }
        }

        Buffer(int session) noexcept
            : id_(std::this_thread::get_id())
            , text_([&] {
                auto buf = std::stringstream{};
                buf << std::hex << id_;

                return buf.str();
            }())
            , session_(session)
            , source_(logger_.CreateBuffer())
        {
        }

        ~Buffer() { logger_.DestroyBuffer(source_.first); }
    };

    static thread_local auto buffer = Buffer{logger_.session_};
    // NOTE this makes logging work if the Context is shutdown then restarted
    buffer.Refresh();
    out = buffer.text_;

    return buffer.source_.second->second;
}

auto Log::Imp::operator()(const std::string_view in) const noexcept
    -> const opentxs::Log&
{
    if (false == active()) { return parent_; }

    auto id = UnallocatedCString{};

    if (auto ticket = logger_.get(); false == ticket) {
        std::get<1>(get_buffer(id)) << in;
    }

    return parent_;
}

auto Log::Imp::operator()(const boost::system::error_code& error) const noexcept
    -> const opentxs::Log&
{
    if (false == active()) { return parent_; }

    auto id = UnallocatedCString{};

    if (auto ticket = logger_.get(); false == ticket) {
        std::get<1>(get_buffer(id)) << error.message();
    }

    return parent_;
}

auto Log::Imp::send(const LogAction action, const Console console)
    const noexcept -> void
{
    if (auto ticket = logger_.get(); false == ticket) {
        send(ticket, action, console);
    }
}

auto Log::Imp::send(
    const Ticket&,
    const LogAction action,
    const Console console) const noexcept -> void
{
    auto id = UnallocatedCString{};
    auto& [socket, buffer] = get_buffer(id);
    // TODO c++20
    socket->Send([&](const auto& buf) {
        auto message = zmq::Message{};
        message.StartBody();
        message.AddFrame(level_);
        message.AddFrame(buf.str());
        message.AddFrame(id);
        message.AddFrame(action);
        message.AddFrame(console);

        return message;
    }(buffer));
    buffer = std::stringstream{};

    if (LogAction::terminate == action) { wait_for_terminate(); }
}

auto Log::Imp::Trace(
    const char* file,
    const std::size_t line,
    const char* message) const noexcept -> void
{
    if (auto ticket = logger_.get(); false == ticket) {
        UnallocatedCString id{};
        auto& [socket, buffer] = get_buffer(id);
        buffer = std::stringstream{};
        buffer << "Stack trace requested";

        if (nullptr != file) { buffer << " in " << file << " line " << line; }

        if (nullptr != message) { buffer << ": " << message; }

        buffer << "\n" << PrintStackTrace();
        send(ticket);
    }
}

auto Log::Imp::wait_for_terminate() const noexcept -> void
{
    Sleep(10s);
    std::abort();
}
}  // namespace opentxs

namespace opentxs
{
Log::Log(const int logLevel) noexcept
    : imp_(std::make_unique<Imp>(logLevel, *this).release())
{
}

auto Log::asHex(const Data& in) const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return (*imp_)(in.asHex());
}

auto Log::asHex(std::string_view in) const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return operator()(
        to_hex(reinterpret_cast<const std::byte*>(in.data()), in.size()));
}

auto Log::operator()() const noexcept -> const Log& { return *this; }

auto Log::operator()(char* in) const noexcept -> const Log&
{
    return operator()(std::string_view{in, std::strlen(in)});
}

auto Log::operator()(const char* in) const noexcept -> const Log&
{
    return operator()(std::string_view{in, std::strlen(in)});
}

auto Log::operator()(const std::string_view in) const noexcept -> const Log&
{
    return (*imp_)(in);
}

auto Log::operator()(const std::filesystem::path in) const noexcept
    -> const Log&
{
    return (*imp_)(in.c_str());
}

auto Log::operator()(const CString& in) const noexcept -> const Log&
{
    return (*imp_)(in);
}

auto Log::operator()(const UnallocatedCString& in) const noexcept -> const Log&
{
    return (*imp_)(in);
}

auto Log::operator()(const std::chrono::nanoseconds& in) const noexcept
    -> const Log&
{
    if (false == imp_->active()) { return *this; }

    auto value = std::stringstream{};
    static constexpr auto nanoThreshold = 2us;
    static constexpr auto microThreshold = 2ms;
    static constexpr auto milliThreshold = 2s;
    static constexpr auto threshold = std::chrono::minutes{2};
    static constexpr auto minThreshold = std::chrono::hours{2};
    static constexpr auto usRatio = 1000ull;
    static constexpr auto msRatio = 1000ull * usRatio;
    static constexpr auto ratio = 1000ull * msRatio;
    static constexpr auto minRatio = 60ull * ratio;
    static constexpr auto hourRatio = 60ull * minRatio;

    if (in < nanoThreshold) {
        value << std::to_string(in.count()) << " nanoseconds";
    } else if (in < microThreshold) {
        value << std::to_string(in.count() / usRatio) << " microseconds";
    } else if (in < milliThreshold) {
        value << std::to_string(in.count() / msRatio) << " milliseconds";
    } else if (in < threshold) {
        value << std::to_string(in.count() / ratio) << " seconds";
    } else if (in < minThreshold) {
        value << std::to_string(in.count() / minRatio) << " minutes";
    } else {
        value << std::to_string(in.count() / hourRatio) << " hours";
    }

    return (*imp_)(value.str());
}

auto Log::operator()(const OTString& in) const noexcept -> const Log&
{
    return (*imp_)(in->Bytes());
}

auto Log::operator()(const OTArmored& in) const noexcept -> const Log&
{
    return operator()(in.get());
}

auto Log::operator()(const Amount& in) const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    const auto intValue = [&] {
        auto out = UnallocatedCString{};
        in.Serialize(opentxs::writer(out));

        return out;
    }();
    const auto floatValue = in.Internal().ToFloat();

    return operator()(floatValue.str() + " (" + intValue + ")");
}

auto Log::operator()(const Amount& in, UnitType currency) const noexcept
    -> const Log&
{
    if (false == imp_->active()) { return *this; }

    if (UnitType::Unknown == currency) { return operator()(in); }

    const auto intValue = [&] {
        auto out = UnallocatedCString{};
        in.Serialize(opentxs::writer(out));

        return out;
    }();

    return operator()(
        display::GetDefinition(currency).Format(in) + " (" + intValue + ")");
}

auto Log::operator()(const Amount& in, const display::Scale& scale)
    const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return operator()(scale.Format(in));
}

auto Log::operator()(const String& in) const noexcept -> const Log&
{
    return operator()(in.Get());
}

auto Log::operator()(const StringXML& in) const noexcept -> const Log&
{
    return operator()(in.Get());
}

auto Log::operator()(const Armored& in) const noexcept -> const Log&
{
    return operator()(in.Get());
}

auto Log::operator()(const identifier::Generic& in) const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return (*imp_)(in.asBase58(Context().Crypto()));
}

auto Log::operator()(const identifier::Nym& in) const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return (*imp_)(in.asBase58(Context().Crypto()));
}

auto Log::operator()(const identifier::Notary& in) const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return (*imp_)(in.asBase58(Context().Crypto()));
}

auto Log::operator()(const identifier::UnitDefinition& in) const noexcept
    -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return (*imp_)(in.asBase58(Context().Crypto()));
}

auto Log::operator()(const Time in) const noexcept -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return operator()(formatTimestamp(in));
}

auto Log::operator()(const boost::system::error_code& error) const noexcept
    -> const Log&
{
    return imp_->operator()(error);
}

auto Log::operator()(const blockchain::block::Outpoint& outpoint) const noexcept
    -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return imp_->operator()(outpoint.str());
}

auto Log::operator()(const blockchain::block::Position& pos) const noexcept
    -> const Log&
{
    if (false == imp_->active()) { return *this; }

    return imp_->operator()(pos.print());
}

auto Log::Assert(const char* file, const std::size_t line) const noexcept
    -> void
{
    Assert(file, line, nullptr);
}

auto Log::Assert(const char* file, const std::size_t line, const char* message)
    const noexcept -> void
{
    imp_->Assert(file, line, message);
}

auto Log::Flush() const noexcept -> void { imp_->Flush(); }

auto Log::Trace(const char* file, const std::size_t line) const noexcept -> void
{
    Trace(file, line, nullptr);
}

auto Log::Trace(const char* file, const std::size_t line, const char* message)
    const noexcept -> void
{
    imp_->Trace(file, line, message);
}

Log::~Log()
{
    if (nullptr != imp_) {
        delete imp_;
        imp_ = nullptr;
    }
}
}  // namespace opentxs

namespace opentxs
{
auto LogAbort() noexcept -> Log&
{
    static auto logger = Log{-2};

    return logger;
}

auto LogConsole() noexcept -> Log&
{
    static auto logger = Log{0};

    return logger;
}

auto LogDebug() noexcept -> Log&
{
    static auto logger = Log{3};

    return logger;
}

auto LogDetail() noexcept -> Log&
{
    static auto logger = Log{1};

    return logger;
}

auto LogError() noexcept -> Log&
{
    static auto logger = Log{-1};

    return logger;
}

auto LogInsane() noexcept -> Log&
{
    static auto logger = Log{5};

    return logger;
}

auto LogTrace() noexcept -> Log&
{
    static auto logger = Log{4};

    return logger;
}

auto LogVerbose() noexcept -> Log&
{
    static auto logger = Log{2};

    return logger;
}

auto PrintStackTrace() noexcept -> UnallocatedCString
{
    auto output = std::stringstream{};
    output << boost::stacktrace::stacktrace();

    return output.str();
}
}  // namespace opentxs
