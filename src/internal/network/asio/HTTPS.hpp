// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include "internal/network/asio/WebRequest.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Container.hpp"
#include "util/Allocated.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace boost
{
namespace asio
{
namespace ssl
{
class context;
}  // namespace ssl
}  // namespace asio
}  // namespace boost

namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace network
{
namespace asio
{
class Context;
}  // namespace asio
}  // namespace network
}  // namespace api

namespace network
{
namespace asio
{
class HTTPS;
}  // namespace asio
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

class opentxs::network::asio::HTTPS final : public WebRequest<HTTPS>
{
public:
    auto Start() noexcept -> void;

    HTTPS(
        const std::string_view hostname,
        const std::string_view file,
        api::network::asio::Context& asio,
        Finish&& cb,
        allocator_type alloc = {})
    noexcept;
    HTTPS(const HTTPS&) = delete;
    HTTPS(HTTPS&) = delete;
    auto operator=(const HTTPS&) -> HTTPS& = delete;
    auto operator=(HTTPS&) -> HTTPS& = delete;

    ~HTTPS() final;

private:
    friend WebRequest<HTTPS>;
    using Stream = beast::ssl_stream<beast::tcp_stream>;

    static auto ssl_certs() -> const Vector<CString>&;

    ssl::context ssl_;
    std::optional<Stream> stream_;

    auto get_stream() noexcept(false) -> Stream&;
    auto get_stream_connect() noexcept(false) -> beast::tcp_stream&;
    auto handshake() noexcept -> void;
    auto load_root_certificates() noexcept(false) -> void;
    auto step_after_connect() noexcept -> void { handshake(); }
};
