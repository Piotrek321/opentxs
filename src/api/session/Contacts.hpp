// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/core/UnitType.hpp"

#pragma once

#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#include "Proto.hpp"
#include "internal/api/session/Contacts.hpp"
#include "internal/util/Editor.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/core/Types.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/identity/wot/claim/ClaimType.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/util/Container.hpp"

namespace opentxs
{
namespace api
{
namespace crypto
{
class Blockchain;
}  // namespace crypto

namespace session
{
class Client;
class Contacts;
class Storage;
}  // namespace session

class Session;
}  // namespace api

namespace identifier
{
class Nym;
}  // namespace identifier

namespace identity
{
class Nym;
}  // namespace identity

namespace proto
{
class Contact;
class Nym;
}  // namespace proto

class Contact;
class PaymentCode;
}  // namespace opentxs

namespace opentxs::api::session::imp
{
class Contacts final : public session::internal::Contacts
{
public:
    auto Contact(const Identifier& id) const
        -> std::shared_ptr<const opentxs::Contact> final;
    auto ContactID(const identifier::Nym& nymID) const -> OTIdentifier final;
    auto ContactList() const -> ObjectList final;
    auto ContactName(const Identifier& contactID) const
        -> UnallocatedCString final;
    auto ContactName(const Identifier& contactID, UnitType currencyHint) const
        -> UnallocatedCString final;
    auto Merge(const Identifier& parent, const Identifier& child) const
        -> std::shared_ptr<const opentxs::Contact> final;
    auto mutable_Contact(const Identifier& id) const
        -> std::unique_ptr<Editor<opentxs::Contact>> final;
    auto NewContact(const UnallocatedCString& label) const
        -> std::shared_ptr<const opentxs::Contact> final;
    auto NewContact(
        const UnallocatedCString& label,
        const identifier::Nym& nymID,
        const PaymentCode& paymentCode) const
        -> std::shared_ptr<const opentxs::Contact> final;
    auto NewContactFromAddress(
        const UnallocatedCString& address,
        const UnallocatedCString& label,
        const opentxs::blockchain::Type currency) const
        -> std::shared_ptr<const opentxs::Contact> final;
    auto NymToContact(const identifier::Nym& nymID) const -> OTIdentifier final;
    auto PaymentCodeToContact(
        const PaymentCode& code,
        const opentxs::blockchain::Type currency) const -> OTIdentifier final;
    auto PaymentCodeToContact(
        const UnallocatedCString& code,
        const opentxs::blockchain::Type currency) const -> OTIdentifier final;
    auto Update(const identity::Nym& nym) const
        -> std::shared_ptr<const opentxs::Contact> final;

    Contacts(const api::session::Client& api);

    ~Contacts() final = default;

private:
    using ContactLock =
        std::pair<std::mutex, std::shared_ptr<opentxs::Contact>>;
    using Address =
        std::pair<identity::wot::claim::ClaimType, UnallocatedCString>;
    using ContactMap = UnallocatedMap<OTIdentifier, ContactLock>;
    using ContactNameMap = UnallocatedMap<OTIdentifier, UnallocatedCString>;

    const api::session::Client& api_;
    mutable std::recursive_mutex lock_{};
    std::weak_ptr<const crypto::Blockchain> blockchain_;
    mutable ContactMap contact_map_{};
    mutable ContactNameMap contact_name_map_;
    OTZMQPublishSocket publisher_;

    void check_identifiers(
        const Identifier& inputNymID,
        const PaymentCode& paymentCode,
        bool& haveNymID,
        bool& havePaymentCode,
        identifier::Nym& outputNymID) const;
    auto verify_write_lock(const rLock& lock) const -> bool;

    // takes ownership
    auto add_contact(const rLock& lock, opentxs::Contact* contact) const
        -> ContactMap::iterator;
    auto contact(const rLock& lock, const UnallocatedCString& label) const
        -> std::shared_ptr<const opentxs::Contact>;
    auto contact(const rLock& lock, const Identifier& id) const
        -> std::shared_ptr<const opentxs::Contact>;
    void import_contacts(const rLock& lock);
    auto init(const std::shared_ptr<const crypto::Blockchain>& blockchain)
        -> void final;
    void init_nym_map(const rLock& lock);
    auto load_contact(const rLock& lock, const Identifier& id) const
        -> ContactMap::iterator;
    auto mutable_contact(const rLock& lock, const Identifier& id) const
        -> std::unique_ptr<Editor<opentxs::Contact>>;
    auto obtain_contact(const rLock& lock, const Identifier& id) const
        -> ContactMap::iterator;
    auto new_contact(
        const rLock& lock,
        const UnallocatedCString& label,
        const identifier::Nym& nymID,
        const PaymentCode& paymentCode) const
        -> std::shared_ptr<const opentxs::Contact>;
    void prepare_shutdown() final { blockchain_.reset(); }
    void refresh_indices(const rLock& lock, opentxs::Contact& contact) const;
    void save(opentxs::Contact* contact) const;
    void start() final;
    auto update_existing_contact(
        const rLock& lock,
        const UnallocatedCString& label,
        const PaymentCode& code,
        const Identifier& contactID) const
        -> std::shared_ptr<const opentxs::Contact>;
    void update_nym_map(
        const rLock& lock,
        const identifier::Nym& nymID,
        opentxs::Contact& contact,
        const bool replace = false) const;

    Contacts() = delete;
    Contacts(const Contacts&) = delete;
    Contacts(Contacts&&) = delete;
    auto operator=(const Contacts&) -> Contacts& = delete;
    auto operator=(Contacts&&) -> Contacts& = delete;
};
}  // namespace opentxs::api::session::imp