// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "opentxs/Version.hpp"
#include "opentxs/blockchain/p2p/Types.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
class Session;
}  // namespace api

namespace blockchain
{
namespace p2p
{
namespace internal
{
struct Address;
}  // namespace internal
}  // namespace p2p
}  // namespace blockchain

namespace network
{
namespace asio
{
class Socket;
}  // namespace asio

namespace zeromq
{
class Frame;
class Message;
}  // namespace zeromq
}  // namespace network

class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::network::blockchain
{
class ConnectionManager
{
public:
    using EndpointData = std::pair<UnallocatedCString, std::uint16_t>;
    using SendPromise = std::promise<bool>;
    using BodySize = std::function<std::size_t(const zeromq::Frame& header)>;
    using Address = opentxs::blockchain::p2p::internal::Address;

    static auto TCP(
        const api::Session& api,
        const Log& log,
        const int id,
        const Address& address,
        const std::size_t headerSize,
        BodySize&& gbs) noexcept -> std::unique_ptr<ConnectionManager>;
    static auto TCPIncoming(
        const api::Session& api,
        const Log& log,
        const int id,
        const Address& address,
        const std::size_t headerSize,
        BodySize&& gbs,
        network::asio::Socket&& socket) noexcept
        -> std::unique_ptr<ConnectionManager>;
    static auto ZMQ(
        const api::Session& api,
        const Log& log,
        const int id,
        const Address& address,
        const std::size_t headerSize) noexcept
        -> std::unique_ptr<ConnectionManager>;
    static auto ZMQIncoming(
        const api::Session& api,
        const Log& log,
        const int id,
        const Address& address,
        const std::size_t headerSize) noexcept
        -> std::unique_ptr<ConnectionManager>;

    virtual auto address() const noexcept -> UnallocatedCString = 0;
    virtual auto endpoint_data() const noexcept -> EndpointData = 0;
    virtual auto host() const noexcept -> UnallocatedCString = 0;
    virtual auto is_initialized() const noexcept -> bool = 0;
    virtual auto port() const noexcept -> std::uint16_t = 0;
    virtual auto style() const noexcept
        -> opentxs::blockchain::p2p::Network = 0;

    virtual auto do_connect() noexcept
        -> std::pair<bool, std::optional<std::string_view>> = 0;
    virtual auto do_init() noexcept -> std::optional<std::string_view> = 0;
    virtual auto on_body(zeromq::Message&&) noexcept
        -> std::optional<zeromq::Message> = 0;
    virtual auto on_connect() noexcept -> void = 0;
    virtual auto on_header(zeromq::Message&&) noexcept
        -> std::optional<zeromq::Message> = 0;
    virtual auto on_init() noexcept -> zeromq::Message = 0;
    virtual auto on_register(zeromq::Message&&) noexcept -> void = 0;
    virtual auto shutdown_external() noexcept -> void = 0;
    virtual auto stop_external() noexcept -> void = 0;
    virtual auto transmit(
        zeromq::Frame&& header,
        zeromq::Frame&& payload,
        std::unique_ptr<SendPromise> promise) noexcept
        -> std::optional<zeromq::Message> = 0;

    virtual ~ConnectionManager() = default;

protected:
    ConnectionManager() = default;
};
}  // namespace opentxs::network::blockchain
