// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/blockchain/BlockchainType.hpp"
// IWYU pragma: no_include "opentxs/blockchain/ClaimType.hpp"

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <memory>

#include "opentxs/blockchain/Types.hpp"
#include "opentxs/core/Types.hpp"
#include "opentxs/util/Types.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace session
{
namespace internal
{
class Contacts;
}  // namespace internal
}  // namespace session
}  // namespace api

namespace identifier
{
class Generic;
class Nym;
}  // namespace identifier

class Contact;
class PaymentCode;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::api::session
{
class OPENTXS_EXPORT Contacts
{
public:
    virtual auto Contact(const identifier::Generic& id) const
        -> std::shared_ptr<const opentxs::Contact> = 0;
    /** Returns the contact ID for a nym, if it exists */
    virtual auto ContactID(const identifier::Nym& nymID) const
        -> identifier::Generic = 0;
    virtual auto ContactList() const -> ObjectList = 0;
    virtual auto ContactName(const identifier::Generic& contactID) const
        -> UnallocatedCString = 0;
    virtual auto ContactName(
        const identifier::Generic& contactID,
        UnitType currencyHint) const -> UnallocatedCString = 0;
    OPENTXS_NO_EXPORT virtual auto Internal() const noexcept
        -> const internal::Contacts& = 0;
    virtual auto Merge(
        const identifier::Generic& parent,
        const identifier::Generic& child) const
        -> std::shared_ptr<const opentxs::Contact> = 0;
    virtual auto NewContact(const UnallocatedCString& label) const
        -> std::shared_ptr<const opentxs::Contact> = 0;
    virtual auto NewContact(
        const UnallocatedCString& label,
        const identifier::Nym& nymID,
        const PaymentCode& paymentCode) const
        -> std::shared_ptr<const opentxs::Contact> = 0;
    virtual auto NewContactFromAddress(
        const UnallocatedCString& address,
        const UnallocatedCString& label,
        const opentxs::blockchain::Type currency) const
        -> std::shared_ptr<const opentxs::Contact> = 0;
    /** Returns an existing contact ID if it exists, or creates a new one */
    virtual auto NymToContact(const identifier::Nym& nymID) const
        -> identifier::Generic = 0;
    /** Returns an existing contact ID if it exists, or creates a new one */
    virtual auto PaymentCodeToContact(
        const PaymentCode& code,
        const opentxs::blockchain::Type currency) const
        -> identifier::Generic = 0;
    virtual auto PaymentCodeToContact(
        const UnallocatedCString& code,
        const opentxs::blockchain::Type currency) const
        -> identifier::Generic = 0;

    OPENTXS_NO_EXPORT virtual auto Internal() noexcept
        -> internal::Contacts& = 0;

    Contacts(const Contacts&) = delete;
    Contacts(Contacts&&) = delete;
    auto operator=(const Contacts&) -> Contacts& = delete;
    auto operator=(Contacts&&) -> Contacts& = delete;

    OPENTXS_NO_EXPORT virtual ~Contacts() = default;

protected:
    Contacts() = default;
};
}  // namespace opentxs::api::session
