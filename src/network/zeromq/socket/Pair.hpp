// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <ostream>
#include <string_view>

#include "internal/util/Mutex.hpp"
#include "network/zeromq/socket/Bidirectional.hpp"
#include "network/zeromq/socket/Receiver.tpp"
#include "network/zeromq/socket/Socket.hpp"
#include "opentxs/network/zeromq/socket/Pair.hpp"
#include "opentxs/network/zeromq/socket/Socket.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "util/Thread.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace network
{
namespace zeromq
{
namespace socket
{
class Pair;
}  // namespace socket

class Context;
class ListenCallback;
class Message;
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::network::zeromq::socket::implementation
{
class Pair final : public Bidirectional<zeromq::socket::Pair>
{
public:
    auto Endpoint() const noexcept -> std::string_view final;
    auto Start(const std::string_view endpoint) const noexcept -> bool final
    {
        return false;
    }

    Pair(
        const zeromq::Context& context,
        const zeromq::ListenCallback& callback,
        const std::string_view endpoint,
        const Direction direction,
        const bool startThreadconst,
        const std::string_view threadName = pairThreadName) noexcept;
    Pair(
        const zeromq::Context& context,
        const zeromq::ListenCallback& callback,
        const bool startThread = true,
        const std::string_view threadName = pairThreadName) noexcept;
    Pair(
        const zeromq::ListenCallback& callback,
        const zeromq::socket::Pair& peer,
        const bool startThread = true,
        const std::string_view threadName = pairThreadName) noexcept;
    Pair(
        const zeromq::Context& context,
        const zeromq::ListenCallback& callback,
        const std::string_view endpoint,
        const std::string_view threadName = pairThreadName) noexcept;
    Pair() = delete;
    Pair(const Pair&) = delete;
    Pair(Pair&&) = delete;
    auto operator=(const Pair&) -> Pair& = delete;
    auto operator=(Pair&&) -> Pair& = delete;

    ~Pair() final;

private:
    const ListenCallback& callback_;
    const CString endpoint_;

    auto clone() const noexcept -> Pair* final;
    auto have_callback() const noexcept -> bool final;
    void process_incoming(const Lock& lock, Message&& message) noexcept final;

    void init() noexcept final;
};
}  // namespace opentxs::network::zeromq::socket::implementation
