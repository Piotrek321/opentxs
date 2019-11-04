// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_NETWORK_SERVERCONNECTION_HPP
#define OPENTXS_NETWORK_SERVERCONNECTION_HPP

#include "opentxs/Forward.hpp"

#include "opentxs/Proto.hpp"
#include "opentxs/Types.hpp"

#include <string>

namespace opentxs
{
namespace api
{
namespace internal
{
struct Core;
}  // namespace internal
}  // namespace api

namespace network
{
class ServerConnection
{
public:
    enum class Push : bool {
        Enable = true,
        Disable = false,
    };

    OPENTXS_EXPORT static OTServerConnection Factory(
        const api::internal::Core& api,
        const api::network::ZMQ& zmq,
        const zeromq::socket::Publish& updates,
        const OTServerContract& contract);

    OPENTXS_EXPORT virtual bool ChangeAddressType(
        const proto::AddressType type) = 0;
    OPENTXS_EXPORT virtual bool ClearProxy() = 0;
    OPENTXS_EXPORT virtual bool EnableProxy() = 0;
    OPENTXS_EXPORT virtual NetworkReplyMessage Send(
        const ServerContext& context,
        const Message& message,
        const PasswordPrompt& reason,
        const Push push = Push::Enable) = 0;
    OPENTXS_EXPORT virtual bool Status() const = 0;

    virtual ~ServerConnection() = default;

protected:
    ServerConnection() = default;

private:
    friend OTServerConnection;

    /** WARNING: not implemented */
    virtual ServerConnection* clone() const = 0;

    ServerConnection(const ServerConnection&) = delete;
    ServerConnection(ServerConnection&&) = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;
    ServerConnection& operator=(ServerConnection&&) = delete;
};
}  // namespace network
}  // namespace opentxs
#endif
