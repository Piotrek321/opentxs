// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cs_plain_guarded.h>
#include <cs_shared_guarded.h>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

#include "internal/otx/common/StringXML.hpp"
#include "internal/util/Log.hpp"
#include "opentxs/core/Armored.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Time.hpp"
#include "util/Gatekeeper.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace boost
{
namespace system
{
class error_code;
}  // namespace system
}  // namespace boost
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs
{
struct Log::Imp final : public internal::Log {
    class Logger
    {
    public:
        using Source = std::pair<OTZMQPushSocket, std::stringstream>;
        using SourceMap = UnallocatedMap<int, Source>;

        std::atomic_int session_{-1};
        std::atomic_int verbosity_{-1};

        auto get() const noexcept -> Ticket;

        auto CreateBuffer() noexcept -> std::pair<int, SourceMap::iterator>;
        auto DestroyBuffer(int index) noexcept -> void;
        auto Start() noexcept -> void;
        auto Stop() noexcept -> void;

    private:
        using Gate = std::optional<Gatekeeper>;

        std::atomic_int index_{-1};
        libguarded::plain_guarded<SourceMap> map_{};
        libguarded::shared_guarded<Gate, std::shared_mutex> gate_{};
    };

    static Logger logger_;

    auto active() const noexcept -> bool;
    auto operator()(const std::string_view in) const noexcept
        -> const opentxs::Log&;
    auto operator()(const boost::system::error_code& error) const noexcept
        -> const opentxs::Log&;

    [[noreturn]] auto Abort() const noexcept -> void;
    [[noreturn]] auto Assert(
        const char* file,
        const std::size_t line,
        const char* message) const noexcept -> void;
    auto Flush() const noexcept -> void;
    auto Trace(const char* file, const std::size_t line, const char* message)
        const noexcept -> void;

    Imp(const int logLevel, opentxs::Log& parent) noexcept;
    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~Imp() final = default;

private:
    const int level_;
    opentxs::Log& parent_;

    static auto get_buffer(UnallocatedCString& id) noexcept -> Logger::Source&;

    auto send(
        const LogAction action = LogAction::flush,
        const Console console = Console::err) const noexcept -> void;
    auto send(
        const Ticket&,
        const LogAction action = LogAction::flush,
        const Console console = Console::err) const noexcept -> void;
    [[noreturn]] auto wait_for_terminate() const noexcept -> void;
};
}  // namespace opentxs
