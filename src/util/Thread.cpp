// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"     // IWYU pragma: associated
#include "1_Internal.hpp"   // IWYU pragma: associated
#include "util/Thread.hpp"  // IWYU pragma: associated

#include <robin_hood.h>

#include "opentxs/util/Log.hpp"

namespace opentxs
{
auto print(ThreadPriority priority) noexcept -> const char*
{
    static const auto map =
        robin_hood::unordered_flat_map<ThreadPriority, const char*>{
            {ThreadPriority::Idle, "idle"},
            {ThreadPriority::Lowest, "lowest"},
            {ThreadPriority::BelowNormal, "below normal"},
            {ThreadPriority::Normal, "normal"},
            {ThreadPriority::AboveNormal, "above normal"},
            {ThreadPriority::Highest, "highest"},
            {ThreadPriority::TimeCritical, "time critical"},
        };

    try {

        return map.at(priority);
    } catch (...) {

        return "error";
    }
}

auto adjustThreadName(
    std::string_view threadName,
    std::string&& appender) noexcept -> std::string
{
    if (auto found = appender.find_first_of(' '); found != std::string::npos) {
        LogError()("Thread name appender contains illegal space: " + appender)
            .Flush();
    }

    int diff = static_cast<int>(threadName.size() + appender.size()) -
               MAX_THREAD_NAME_SIZE;
    std::string name{threadName};
    if (diff <= 0) {
        name.append(appender);
        return name;
    }

    if (diff >= static_cast<int>(appender.size())) return name;

    name.append(appender.substr(0, appender.size() - diff));
    return name;
}
}  // namespace opentxs
