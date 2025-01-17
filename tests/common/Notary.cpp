// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "common/Notary.hpp"  // IWYU pragma: associated

#include <future>
#include <memory>
#include <utility>

#include "integration/Helpers.hpp"
#include "internal/otx/common/Message.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Notary.hpp"
#include "opentxs/api/session/OTX.hpp"
#include "opentxs/api/session/Wallet.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/core/contract/Unit.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/otx/LastReplyStatus.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/SharedPimpl.hpp"

namespace ottest
{
Notary_fixture::IssuedUnits Notary_fixture::created_units_{};
Notary_fixture::AccountMap Notary_fixture::registered_accounts_{};
Notary_fixture::AccountIndex Notary_fixture::account_index_{};

auto Notary_fixture::CleanupNotary() noexcept -> void
{
    registered_accounts_.clear();
    created_units_.clear();
    account_index_.clear();
}

auto Notary_fixture::IssueUnit(
    const ot::api::session::Notary& server,
    const User& issuer,
    const ot::UnallocatedCString& shortname,
    const ot::UnallocatedCString& terms,
    ot::UnitType unitOfAccount,
    const ot::display::Definition& displayDefinition) const noexcept
    -> ot::UnallocatedCString
{
    return IssueUnit(
        *issuer.api_,
        server,
        issuer.nym_id_,
        shortname,
        terms,
        unitOfAccount,
        displayDefinition);
}

auto Notary_fixture::IssueUnit(
    const ot::api::session::Client& api,
    const ot::api::session::Notary& server,
    const ot::identifier::Nym& nymID,
    const ot::UnallocatedCString& shortname,
    const ot::UnallocatedCString& terms,
    ot::UnitType unitOfAccount,
    const ot::display::Definition& displayDefinition) const noexcept
    -> ot::UnallocatedCString
{
    static auto accountIndex = int{0};

    const auto& serverID = server.ID();
    const auto reason = api.Factory().PasswordPrompt(__func__);
    const auto contract = api.Wallet().CurrencyContract(
        nymID.str(),
        shortname,
        terms,
        unitOfAccount,
        1,
        displayDefinition,
        reason);

    if (0u == contract->Version()) { return {}; }

    const auto& output = created_units_.emplace_back(contract->ID()->str());
    const auto unitID = api.Factory().UnitID(output);
    auto [taskID, future] = api.OTX().IssueUnitDefinition(
        nymID, serverID, unitID, unitOfAccount, "issuer account");

    if (0 == taskID) { return {}; }

    const auto [status, message] = future.get();

    if (ot::otx::LastReplyStatus::MessageSuccess != status) { return {}; }

    const auto& accountID = registered_accounts_[nymID.str()].emplace_back(
        message->m_strAcctID->Get());
    account_index_.emplace(unitOfAccount, accountIndex++);

    if (accountID.empty()) { return {}; }

    RefreshAccount(api, nymID, serverID);

    return output;
}

auto Notary_fixture::RefreshAccount(
    const ot::api::session::Client& api,
    const ot::identifier::Nym& nym,
    const ot::identifier::Notary& server) const noexcept -> void
{
    api.OTX().Refresh();
    api.OTX().ContextIdle(nym, server).get();
}

auto Notary_fixture::RegisterNym(
    const ot::api::session::Client& api,
    const ot::api::session::Notary& server,
    const ot::UnallocatedCString& nymID) const noexcept -> bool
{
    return RegisterNym(api, server, api.Factory().NymID(nymID));
}

auto Notary_fixture::RegisterNym(
    const ot::api::session::Notary& server,
    const User& nym) const noexcept -> bool
{
    return RegisterNym(*nym.api_, server, nym.nym_id_);
}

auto Notary_fixture::RegisterNym(
    const ot::api::session::Client& api,
    const ot::api::session::Notary& server,
    const ot::identifier::Nym& nymID) const noexcept -> bool
{
    const auto& serverID = server.ID();
    auto [taskID, future] = api.OTX().RegisterNymPublic(nymID, serverID, true);

    if (0 == taskID) { return false; }

    const auto [status, message] = future.get();

    if (ot::otx::LastReplyStatus::MessageSuccess != status) { return false; }

    RefreshAccount(api, nymID, serverID);

    return true;
}

auto Notary_fixture::StartNotarySession(int index) const noexcept
    -> const ot::api::session::Notary&
{
    const auto& out = ot_.StartNotarySession(index);
    return out;
}
}  // namespace ottest
