// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <memory>

#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace network
{
class Asio;
class Network;
}  // namespace network

namespace session
{
class Endpoints;
class Scheduler;
}  // namespace session

class Session;
}  // namespace api

namespace network
{
namespace zeromq
{
class Context;
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::factory
{
auto BlockchainNetworkAPI(
    const api::Session& api,
    const api::session::Endpoints& endpoints,
    const opentxs::network::zeromq::Context& zmq) noexcept
    -> api::network::Blockchain::Imp*;
auto BlockchainNetworkAPINull() noexcept -> api::network::Blockchain::Imp*;
auto NetworkAPI(
    const api::Session& api,
    const api::network::Asio& asio,
    const network::zeromq::Context& zmq,
    const api::session::Endpoints& endpoints,
    api::network::Blockchain::Imp* blockchain) noexcept
    -> api::network::Network::Imp*;
}  // namespace opentxs::factory
