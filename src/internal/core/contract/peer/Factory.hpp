// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include "opentxs/core/contract/peer/PeerReply.hpp"
#include "opentxs/core/contract/peer/PeerRequest.hpp"
#include "opentxs/identity/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace session
{
class Client;
}  // namespace session

class Session;
}  // namespace api

namespace otx
{
namespace blind
{
class Purse;
}  // namespace blind
}  // namespace otx

namespace proto
{
class PeerObject;
}  // namespace proto

class Armored;
class PasswordPrompt;
class PeerObject;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::factory
{
auto PeerObject(
    const api::Session& api,
    const Nym_p& senderNym,
    const UnallocatedCString& message) noexcept
    -> std::unique_ptr<opentxs::PeerObject>;
auto PeerObject(
    const api::Session& api,
    const Nym_p& senderNym,
    const UnallocatedCString& payment,
    const bool isPayment) noexcept -> std::unique_ptr<opentxs::PeerObject>;
auto PeerObject(
    const api::Session& api,
    const Nym_p& senderNym,
    otx::blind::Purse&& purse) noexcept -> std::unique_ptr<opentxs::PeerObject>;
auto PeerObject(
    const api::Session& api,
    const OTPeerRequest request,
    const OTPeerReply reply,
    const VersionNumber version) noexcept
    -> std::unique_ptr<opentxs::PeerObject>;
auto PeerObject(
    const api::Session& api,
    const OTPeerRequest request,
    const VersionNumber version) noexcept
    -> std::unique_ptr<opentxs::PeerObject>;
auto PeerObject(
    const api::session::Client& api,
    const Nym_p& signerNym,
    const proto::PeerObject& serialized) noexcept
    -> std::unique_ptr<opentxs::PeerObject>;
auto PeerObject(
    const api::session::Client& api,
    const Nym_p& recipientNym,
    const opentxs::Armored& encrypted,
    const opentxs::PasswordPrompt& reason) noexcept
    -> std::unique_ptr<opentxs::PeerObject>;
auto PeerReply(const api::Session& api) noexcept
    -> std::unique_ptr<contract::peer::Reply>;

auto PeerRequest(const api::Session& api) noexcept
    -> std::unique_ptr<contract::peer::Request>;
}  // namespace opentxs::factory
