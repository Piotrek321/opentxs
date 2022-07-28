// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                       // IWYU pragma: associated
#include "1_Internal.hpp"                     // IWYU pragma: associated
#include "opentxs/network/zeromq/ZeroMQ.hpp"  // IWYU pragma: associated

#include <zmq.h>
#include <atomic>
#include <cstdint>
#include <sstream>

#include "internal/util/P0330.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::network::zeromq
{
using namespace std::literals;

constexpr auto inproc_prefix_{"inproc://opentxs/"sv};
constexpr auto path_seperator_{"/"sv};

auto MakeArbitraryInproc() noexcept -> UnallocatedCString
{
    static auto counter = std::atomic_int{0};
    auto out = std::stringstream{};
    out << inproc_prefix_;
    out << "arbitrary"sv;
    out << path_seperator_;
    out << std::to_string(++counter);

    return out.str();
}

auto MakeArbitraryInproc(alloc::Resource* alloc) noexcept -> CString
{
    const auto data = MakeArbitraryInproc();
    auto out = CString{alloc};
    out.assign(data.data(), data.size());

    return out;
}

auto MakeDeterministicInproc(
    const std::string_view path,
    const int instance,
    const int version) noexcept -> UnallocatedCString
{
    auto out = std::stringstream{};
    out << inproc_prefix_;
    out << std::to_string(instance);
    out << path_seperator_;
    out << path;
    out << path_seperator_;
    out << std::to_string(version);

    return out.str();
}

auto MakeDeterministicInproc(
    const std::string_view path,
    const int instance,
    const int version,
    const std::string_view suffix) noexcept -> UnallocatedCString
{
    auto out = std::stringstream{};
    out << MakeDeterministicInproc(path, instance, version);
    out << path_seperator_;
    out << suffix;

    return out.str();
}

auto RawToZ85(const ReadView input, const AllocateOutput destination) noexcept
    -> bool
{
    if (0 != input.size() % 4) {
        LogError()("opentxs::network::zeromq::")(__func__)(
            ": Invalid input size.")
            .Flush();

        return false;
    }

    if (false == bool(destination)) {
        LogError()("opentxs::network::zeromq::")(__func__)(
            ": Invalid output allocator.")
            .Flush();

        return false;
    }

    const auto target = input.size() + input.size() / 4_uz + 1_uz;
    auto out = destination(target);

    if (false == out.valid(target)) {
        LogError()("opentxs::network::zeromq::")(__func__)(
            ": Failed to allocate output")
            .Flush();

        return false;
    }

    return nullptr != ::zmq_z85_encode(
                          out.as<char>(),
                          reinterpret_cast<const std::uint8_t*>(input.data()),
                          input.size());
}

auto Z85ToRaw(const ReadView input, const AllocateOutput destination) noexcept
    -> bool
{
    if (0 != input.size() % 5) {
        LogError()("opentxs::network::zeromq::")(__func__)(
            ": Invalid input size.")
            .Flush();

        return false;
    }

    if (false == bool(destination)) {
        LogError()("opentxs::network::zeromq::")(__func__)(
            ": Invalid output allocator.")
            .Flush();

        return false;
    }

    const auto target = input.size() * 4_uz / 5_uz;
    auto out = destination(target);

    if (false == out.valid(target)) {
        LogError()("opentxs::network::zeromq::")(__func__)(
            ": Failed to allocate output")
            .Flush();

        return false;
    }

    return ::zmq_z85_decode(out.as<std::uint8_t>(), input.data());
}
}  // namespace opentxs::network::zeromq
