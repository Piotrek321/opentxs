// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/core/UnitType.hpp"

#pragma once

#include <StorageAccounts.pb.h>
#include <tuple>

#include "Proto.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/core/Types.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Notary.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/util/Container.hpp"
#include "util/storage/tree/Node.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace storage
{
class Driver;
class Tree;
}  // namespace storage
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::storage
{
class Accounts final : public Node
{
public:
    auto AccountContract(const Identifier& accountID) const -> OTUnitID;
    auto AccountIssuer(const Identifier& accountID) const -> OTNymID;
    auto AccountOwner(const Identifier& accountID) const -> OTNymID;
    auto AccountServer(const Identifier& accountID) const -> OTNotaryID;
    auto AccountSigner(const Identifier& accountID) const -> OTNymID;
    auto AccountUnit(const Identifier& accountID) const -> UnitType;
    auto AccountsByContract(const identifier::UnitDefinition& unit) const
        -> UnallocatedSet<OTIdentifier>;
    auto AccountsByIssuer(const identifier::Nym& issuerNym) const
        -> UnallocatedSet<OTIdentifier>;
    auto AccountsByOwner(const identifier::Nym& ownerNym) const
        -> UnallocatedSet<OTIdentifier>;
    auto AccountsByServer(const identifier::Notary& server) const
        -> UnallocatedSet<OTIdentifier>;
    auto AccountsByUnit(const UnitType unit) const
        -> UnallocatedSet<OTIdentifier>;
    auto Alias(const UnallocatedCString& id) const -> UnallocatedCString;
    auto Load(
        const UnallocatedCString& id,
        UnallocatedCString& output,
        UnallocatedCString& alias,
        const bool checking) const -> bool;

    auto Delete(const UnallocatedCString& id) -> bool;
    auto SetAlias(const UnallocatedCString& id, const UnallocatedCString& alias)
        -> bool;
    auto Store(
        const UnallocatedCString& id,
        const UnallocatedCString& data,
        const UnallocatedCString& alias,
        const identifier::Nym& ownerNym,
        const identifier::Nym& signerNym,
        const identifier::Nym& issuerNym,
        const identifier::Notary& server,
        const identifier::UnitDefinition& contract,
        const UnitType unit) -> bool;

    Accounts() = delete;
    Accounts(const Accounts&) = delete;
    Accounts(Accounts&&) = delete;
    auto operator=(const Accounts&) -> Accounts = delete;
    auto operator=(Accounts&&) -> Accounts = delete;

    ~Accounts() final = default;

private:
    friend Tree;

    using NymIndex = UnallocatedMap<OTNymID, UnallocatedSet<OTIdentifier>>;
    using ServerIndex =
        UnallocatedMap<OTNotaryID, UnallocatedSet<OTIdentifier>>;
    using ContractIndex =
        UnallocatedMap<OTUnitID, UnallocatedSet<OTIdentifier>>;
    using UnitIndex = UnallocatedMap<UnitType, UnallocatedSet<OTIdentifier>>;
    /** owner, signer, issuer, server, contract, unit */
    using AccountData =
        std::tuple<OTNymID, OTNymID, OTNymID, OTNotaryID, OTUnitID, UnitType>;
    using ReverseIndex = UnallocatedMap<OTIdentifier, AccountData>;

    NymIndex owner_index_{};
    NymIndex signer_index_{};
    NymIndex issuer_index_{};
    ServerIndex server_index_{};
    ContractIndex contract_index_{};
    UnitIndex unit_index_{};
    mutable ReverseIndex account_data_{};

    template <typename A, typename M, typename I>
    static auto add_set_index(
        const Identifier& accountID,
        const A& argID,
        M& mapID,
        I& index) -> bool;

    template <typename K, typename I>
    static void erase(const Identifier& accountID, const K& key, I& index)
    {
        try {
            auto& set = index.at(key);
            set.erase(accountID);

            if (0 == set.size()) { index.erase(key); }
        } catch (...) {
        }
    }

    auto get_account_data(const Lock& lock, const OTIdentifier& accountID) const
        -> AccountData&;
    auto serialize() const -> proto::StorageAccounts;

    auto check_update_account(
        const Lock& lock,
        const OTIdentifier& accountID,
        const identifier::Nym& ownerNym,
        const identifier::Nym& signerNym,
        const identifier::Nym& issuerNym,
        const identifier::Notary& server,
        const identifier::UnitDefinition& contract,
        const UnitType unit) -> bool;
    void init(const UnallocatedCString& hash) final;
    auto save(const Lock& lock) const -> bool final;

    Accounts(const Driver& storage, const UnallocatedCString& key);
};
}  // namespace opentxs::storage
