// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <memory>

#include "opentxs/core/contract/Signable.hpp"
#include "opentxs/crypto/key/asymmetric/Role.hpp"
#include "opentxs/identity/Types.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace identity
{
namespace credential
{
namespace internal
{
class Base;
}  // namespace internal
}  // namespace credential
}  // namespace identity

class PasswordPrompt;
class Secret;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::identity::credential
{
class OPENTXS_EXPORT Base : virtual public opentxs::contract::Signable
{
public:
    virtual auto asString(const bool asPrivate = false) const
        -> UnallocatedCString = 0;
    virtual auto CredentialID() const -> const identifier::Generic& = 0;
    virtual auto hasCapability(const NymCapability& capability) const
        -> bool = 0;
    OPENTXS_NO_EXPORT virtual auto Internal() const noexcept
        -> const internal::Base& = 0;
    virtual auto MasterSignature() const -> Signature = 0;
    virtual auto Mode() const -> crypto::key::asymmetric::Mode = 0;
    virtual auto Role() const -> identity::CredentialRole = 0;
    virtual auto Private() const -> bool = 0;
    virtual auto Save() const -> bool = 0;
    virtual auto SourceSignature() const -> Signature = 0;
    virtual auto TransportKey(
        Data& publicKey,
        Secret& privateKey,
        const PasswordPrompt& reason) const -> bool = 0;
    virtual auto Type() const -> identity::CredentialType = 0;

    OPENTXS_NO_EXPORT virtual auto Internal() noexcept -> internal::Base& = 0;

    Base(const Base&) = delete;
    Base(Base&&) = delete;
    auto operator=(const Base&) -> Base& = delete;
    auto operator=(Base&&) -> Base& = delete;

    ~Base() override = default;

protected:
    Base() noexcept = default;  // TODO Signable
};
}  // namespace opentxs::identity::credential
