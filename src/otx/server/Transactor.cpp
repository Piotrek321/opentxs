// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"               // IWYU pragma: associated
#include "1_Internal.hpp"             // IWYU pragma: associated
#include "otx/server/Transactor.hpp"  // IWYU pragma: associated

#include <memory>
#include <utility>

#include "internal/otx/AccountList.hpp"
#include "internal/otx/common/Account.hpp"
#include "internal/util/Exclusive.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Notary.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/otx/consensus/Client.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "otx/server/MainFile.hpp"
#include "otx/server/Server.hpp"

namespace opentxs::server
{
Transactor::Transactor(Server& server, const PasswordPrompt& reason)
    : server_(server)
    , reason_(reason)
    , transactionNumber_(0)
    , idToBasketMap_()
    , contractIdToBasketAccountId_()
    , voucherAccounts_(server.API())
{
}

/// Just as every request must be accompanied by a request number, so
/// every transaction request must be accompanied by a transaction number.
/// The request numbers can simply be incremented on both sides (per user.)
/// But the transaction numbers must be issued by the server and they do
/// not repeat from user to user. They are unique to transaction.
///
/// Users must ask the server to send them transaction numbers so that they
/// can be used in transaction requests.
auto Transactor::issueNextTransactionNumber(
    TransactionNumber& lTransactionNumber) -> bool
{
    // transactionNumber_ stores the last VALID AND ISSUED transaction number.
    // So first, we increment that, since we don't want to issue the same number
    // twice.
    transactionNumber_++;

    // Next, we save it to file.
    if (!server_.GetMainFile().SaveMainFile()) {
        LogError()(OT_PRETTY_CLASS())("Error saving main server file.").Flush();
        transactionNumber_--;
        return false;
    }

    // SUCCESS?
    // Now the server main file has saved the latest transaction number,
    // NOW we set it onto the parameter and return true.
    lTransactionNumber = transactionNumber_;
    return true;
}

auto Transactor::issueNextTransactionNumberToNym(
    otx::context::Client& context,
    TransactionNumber& lTransactionNumber) -> bool
{
    if (!issueNextTransactionNumber(lTransactionNumber)) { return false; }

    // Each Nym stores the transaction numbers that have been issued to it.
    // (On client AND server side.)
    //
    // So whenever the server issues a new number, it's to a specific Nym, then
    // it is recorded in his Nym file before being sent to the client (where it
    // is also recorded in his Nym file.)  That way the server always knows
    // which numbers are valid for each Nym.
    if (!context.IssueNumber(transactionNumber_)) {
        LogError()(OT_PRETTY_CLASS())(
            ": Error adding transaction number to Nym file.")
            .Flush();
        transactionNumber_--;
        // Save it back how it was, since we're not issuing this number after
        // all.
        server_.GetMainFile().SaveMainFile();

        return false;
    }

    // SUCCESS?
    // Now the server main file has saved the latest transaction number,
    // NOW we set it onto the parameter and return true.
    lTransactionNumber = transactionNumber_;

    return true;
}

// Server stores a map of BASKET_ID to BASKET_ACCOUNT_ID.
auto Transactor::addBasketAccountID(
    const identifier::Generic& BASKET_ID,
    const identifier::Generic& BASKET_ACCOUNT_ID,
    const identifier::Generic& BASKET_CONTRACT_ID) -> bool
{
    auto theBasketAcctID = identifier::Generic{};

    if (lookupBasketAccountID(BASKET_ID, theBasketAcctID)) {
        {
            LogConsole()(OT_PRETTY_CLASS())(
                ": User attempted to add Basket that already exists.")
                .Flush();
        }

        return false;
    }

    auto strBasketID = String::Factory(BASKET_ID),
         strBasketAcctID = String::Factory(BASKET_ACCOUNT_ID),
         strBasketContractID = String::Factory(BASKET_CONTRACT_ID);

    idToBasketMap_[strBasketID->Get()] = strBasketAcctID->Get();
    contractIdToBasketAccountId_[strBasketContractID->Get()] =
        strBasketAcctID->Get();

    return true;
}

/// Use this to find the basket account ID for this server (which is unique to
/// this server)
/// using the contract ID to look it up. (The basket contract ID is unique to
/// this server.)
auto Transactor::lookupBasketAccountIDByContractID(
    const identifier::Generic& BASKET_CONTRACT_ID,
    identifier::Generic& BASKET_ACCOUNT_ID) -> bool
{
    // Server stores a map of BASKET_ID to BASKET_ACCOUNT_ID. Let's iterate
    // through that map...
    for (auto& it : contractIdToBasketAccountId_) {
        auto id_BASKET_CONTRACT =
            server_.API().Factory().IdentifierFromBase58(it.first);
        auto id_BASKET_ACCT =
            server_.API().Factory().IdentifierFromBase58(it.second);

        if (BASKET_CONTRACT_ID == id_BASKET_CONTRACT)  // if the basket contract
                                                       // ID passed in matches
                                                       // this one...
        {
            BASKET_ACCOUNT_ID.Assign(id_BASKET_ACCT);
            return true;
        }
    }
    return false;
}

/// Use this to find the basket account ID for this server (which is unique to
/// this server)
/// using the contract ID to look it up. (The basket contract ID is unique to
/// this server.)
auto Transactor::lookupBasketContractIDByAccountID(
    const identifier::Generic& BASKET_ACCOUNT_ID,
    identifier::Generic& BASKET_CONTRACT_ID) -> bool
{
    // Server stores a map of BASKET_ID to BASKET_ACCOUNT_ID. Let's iterate
    // through that map...
    for (auto& it : contractIdToBasketAccountId_) {
        auto id_BASKET_CONTRACT =
            server_.API().Factory().IdentifierFromBase58(it.first);
        auto id_BASKET_ACCT =
            server_.API().Factory().IdentifierFromBase58(it.second);

        if (BASKET_ACCOUNT_ID == id_BASKET_ACCT)  // if the basket contract ID
                                                  // passed in matches this
                                                  // one...
        {
            BASKET_CONTRACT_ID.Assign(id_BASKET_CONTRACT);
            return true;
        }
    }
    return false;
}

/// Use this to find the basket account for this server (which is unique to this
/// server)
/// using the basket ID to look it up (the Basket ID is the same for all
/// servers)
auto Transactor::lookupBasketAccountID(
    const identifier::Generic& BASKET_ID,
    identifier::Generic& BASKET_ACCOUNT_ID) -> bool
{
    // Server stores a map of BASKET_ID to BASKET_ACCOUNT_ID. Let's iterate
    // through that map...
    for (auto& it : idToBasketMap_) {
        auto id_BASKET = server_.API().Factory().IdentifierFromBase58(it.first);
        auto id_BASKET_ACCT =
            server_.API().Factory().IdentifierFromBase58(it.second);

        if (BASKET_ID == id_BASKET)  // if the basket ID passed in matches this
                                     // one...
        {
            BASKET_ACCOUNT_ID.Assign(id_BASKET_ACCT);
            return true;
        }
    }
    return false;
}

/// Looked up the voucher account (where cashier's cheques are issued for any
/// given instrument definition) return a pointer to the account.  Since it's
/// SUPPOSED to
/// exist, and since it's being requested, also will GENERATE it if it cannot
/// be found, add it to the list, and return the pointer. Should always succeed.
auto Transactor::getVoucherAccount(
    const identifier::UnitDefinition& INSTRUMENT_DEFINITION_ID)
    -> ExclusiveAccount
{
    const auto& NOTARY_NYM_ID = server_.GetServerNym().ID();
    const auto& NOTARY_ID = server_.GetServerID();
    bool bWasAcctCreated = false;
    auto pAccount = voucherAccounts_.GetOrRegisterAccount(
        server_.GetServerNym(),
        NOTARY_NYM_ID,
        INSTRUMENT_DEFINITION_ID,
        NOTARY_ID,
        bWasAcctCreated,
        reason_);
    if (bWasAcctCreated) {
        auto strAcctID = String::Factory();
        pAccount.get().GetIdentifier(strAcctID);
        const auto strInstrumentDefinitionID =
            String::Factory(INSTRUMENT_DEFINITION_ID);
        {
            LogConsole()(OT_PRETTY_CLASS())("Successfully created "
                                            "voucher account ID: ")(
                strAcctID)(" Instrument Definition "
                           "ID:"
                           " ")(strInstrumentDefinitionID)(".")
                .Flush();
        }
        if (!server_.GetMainFile().SaveMainFile()) {
            LogError()(OT_PRETTY_CLASS())(
                ": Error saving main "
                "server file containing new account ID!!")
                .Flush();
        }
    }

    return pAccount;
}
}  // namespace opentxs::server
