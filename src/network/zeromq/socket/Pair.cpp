// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                    // IWYU pragma: associated
#include "1_Internal.hpp"                  // IWYU pragma: associated
#include "network/zeromq/socket/Pair.hpp"  // IWYU pragma: associated

#include <memory>
#include <utility>

#include "internal/network/zeromq/socket/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "network/zeromq/socket/Bidirectional.tpp"
#include "network/zeromq/socket/Receiver.hpp"
#include "network/zeromq/socket/Receiver.tpp"
#include "network/zeromq/socket/Sender.tpp"
#include "network/zeromq/socket/Socket.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/ZeroMQ.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/Pair.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"
#include "opentxs/util/Pimpl.hpp"

template class opentxs::Pimpl<opentxs::network::zeromq::socket::Pair>;

namespace opentxs::factory
{
auto PairSocket(
    const network::zeromq::Context& context,
    const network::zeromq::ListenCallback& callback,
    const bool startThread,
    const std::string_view threadName)
    -> std::unique_ptr<network::zeromq::socket::Pair>
{
    using ReturnType = network::zeromq::socket::implementation::Pair;

    return std::make_unique<ReturnType>(
        context, callback, startThread, threadName);
}

auto PairSocket(
    const network::zeromq::ListenCallback& callback,
    const network::zeromq::socket::Pair& peer,
    const bool startThread,
    const std::string_view threadName)
    -> std::unique_ptr<network::zeromq::socket::Pair>
{
    using ReturnType = network::zeromq::socket::implementation::Pair;

    return std::make_unique<ReturnType>(
        callback, peer, startThread, threadName);
}

auto PairSocket(
    const network::zeromq::Context& context,
    const network::zeromq::ListenCallback& callback,
    const std::string_view endpoint,
    const std::string_view threadName)
    -> std::unique_ptr<network::zeromq::socket::Pair>
{
    using ReturnType = network::zeromq::socket::implementation::Pair;

    return std::make_unique<ReturnType>(
        context, callback, endpoint, threadName);
}
}  // namespace opentxs::factory

namespace opentxs::network::zeromq::socket::implementation
{
Pair::Pair(
    const zeromq::Context& context,
    const zeromq::ListenCallback& callback,
    const std::string_view endpoint,
    const Direction direction,
    const bool startThread,
    const std::string_view threadName) noexcept
    : Receiver(
          context,
          socket::Type::Pair,
          direction,
          startThread,
          adjustThreadName(threadName, pairThreadName.data()))
    , Bidirectional(
          context,
          true,
          adjustThreadName(threadName, pairThreadName.data()))
    , callback_(callback)
    , endpoint_(endpoint)
{
    init();
}

Pair::Pair(
    const zeromq::Context& context,
    const zeromq::ListenCallback& callback,
    const bool startThread,
    const std::string_view threadName) noexcept
    : Pair(
          context,
          callback,
          MakeArbitraryInproc(),
          Direction::Bind,
          startThread,
          threadName)
{
}

Pair::Pair(
    const zeromq::ListenCallback& callback,
    const zeromq::socket::Pair& peer,
    const bool startThread,
    const std::string_view threadName) noexcept
    : Pair(
          peer.Context(),
          callback,
          peer.Endpoint(),
          Direction::Connect,
          startThread,
          threadName)
{
}

Pair::Pair(
    const zeromq::Context& context,
    const zeromq::ListenCallback& callback,
    const std::string_view endpoint,
    const std::string_view threadName) noexcept
    : Pair(context, callback, endpoint, Direction::Connect, true, threadName)
{
}

auto Pair::clone() const noexcept -> Pair*
{
    return new Pair(context_, callback_, endpoint_, direction_, false);
}

auto Pair::Endpoint() const noexcept -> std::string_view { return endpoint_; }

auto Pair::have_callback() const noexcept -> bool { return true; }

void Pair::init() noexcept
{
    Bidirectional::init();

    OT_ASSERT(false == endpoint_.empty())

    const auto init = Bidirectional::Start(endpoint_);

    OT_ASSERT(init)
}

void Pair::process_incoming(const Lock& lock, Message&& message) noexcept
{
    OT_ASSERT(verify_lock(lock))

    callback_.Process(std::move(message));
}

Pair::~Pair() SHUTDOWN_SOCKET
}  // namespace opentxs::network::zeromq::socket::implementation
