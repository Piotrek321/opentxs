// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <mutex>

#include "opentxs/Types.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/util/Container.hpp"

namespace opentxs
{
namespace api
{
class Legacy;
}  // namespace api

namespace crypto
{
namespace key
{
class Symmetric;
}  // namespace key
}  // namespace crypto

namespace identifier
{
class Nym;
}  // namespace identifier

class Secret;
class PasswordPrompt;
}  // namespace opentxs

namespace
{
/** Callbacks in this form allow OpenSSL to query opentxs to get key encryption
 *  and decryption passwords*/
extern "C" {
using INTERNAL_PASSWORD_CALLBACK =
    std::int32_t(char*, std::int32_t, std::int32_t, void*);
}
}  // namespace

namespace opentxs::api::session::internal
{
class Session : virtual public api::Session
{
public:
    virtual auto GetInternalPasswordCallback() const
        -> INTERNAL_PASSWORD_CALLBACK* = 0;
    virtual auto GetSecret(
        const opentxs::Lock& lock,
        Secret& secret,
        const PasswordPrompt& reason,
        const bool twice,
        const UnallocatedCString& key = "") const -> bool = 0;
    auto Internal() const noexcept -> const Session& final { return *this; }
    virtual auto Legacy() const noexcept -> const api::Legacy& = 0;
    virtual auto Lock() const -> std::mutex& = 0;
    virtual auto MasterKey(const opentxs::Lock& lock) const
        -> const opentxs::crypto::key::Symmetric& = 0;
    virtual auto NewNym(const identifier::Nym& id) const noexcept -> void = 0;

    auto Internal() noexcept -> Session& final { return *this; }

    ~Session() override = default;
};
}  // namespace opentxs::api::session::internal