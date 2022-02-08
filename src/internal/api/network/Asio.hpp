// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <future>

#include "opentxs/network/asio/Endpoint.hpp"
#include "opentxs/network/asio/Socket.hpp"
#include "opentxs/util/Bytes.hpp"
#include "util/Work.hpp"

namespace boost
{
namespace asio
{
class io_context;
}  // namespace asio

namespace json
{
class value;
}  // namespace json
}  // namespace boost

namespace opentxs
{
class Timer;
}  // namespace opentxs

namespace opentxs
{
enum class ThreadPool {
    General,
    Network,
    Storage,
    Blockchain,
};
}  // namespace opentxs

namespace opentxs::api::network::internal
{
class Asio
{
public:
    using Endpoint = opentxs::network::asio::Endpoint::Imp;
    using Socket = opentxs::network::asio::Socket::Imp;
    using Callback = std::function<void()>;

    virtual auto FetchJson(
        const ReadView host,
        const ReadView path,
        const bool https = true,
        const ReadView notify = {}) const noexcept
        -> std::future<boost::json::value> = 0;

    virtual auto Connect(const ReadView id, Socket& socket) noexcept
        -> bool = 0;
    virtual auto GetTimer() noexcept -> Timer = 0;
    virtual auto IOContext() noexcept -> boost::asio::io_context& = 0;
    virtual auto Post(ThreadPool type, Callback cb) noexcept -> bool = 0;
    virtual auto Receive(
        const ReadView id,
        const OTZMQWorkType type,
        const std::size_t bytes,
        Socket& socket) noexcept -> bool = 0;

    virtual ~Asio() = default;

protected:
    Asio() = default;

private:
    Asio(const Asio&) = delete;
    Asio(Asio&&) = delete;
    Asio& operator=(const Asio&) = delete;
    Asio& operator=(Asio&&) = delete;
};
}  // namespace opentxs::api::network::internal