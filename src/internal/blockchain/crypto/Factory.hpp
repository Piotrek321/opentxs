// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/blockchain/crypto/HDProtocol.hpp"

#pragma once

#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace crypto
{
class Blockchain;
}  // namespace crypto

namespace session
{
class Contacts;
}  // namespace session

class Session;
}  // namespace api

namespace blockchain
{
namespace crypto
{
class Account;
class AccountIndex;
class HD;
class Notification;
class PaymentCode;
class Wallet;
}  // namespace crypto
}  // namespace blockchain

namespace identifier
{
class Generic;
class Nym;
}  // namespace identifier

namespace identity
{
class Nym;
}  // namespace identity

namespace proto
{
class Bip47Channel;
class HDAccount;
class HDPath;
}  // namespace proto

class Data;
class PasswordPrompt;
class PaymentCode;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::factory
{
auto BlockchainAccountKeys(
    const api::Session& api,
    const api::session::Contacts& contacts,
    const blockchain::crypto::Wallet& parent,
    const blockchain::crypto::AccountIndex& index,
    const identifier::Nym& id,
    const UnallocatedSet<identifier::Generic>& hdAccounts,
    const UnallocatedSet<identifier::Generic>& importedAccounts,
    const UnallocatedSet<identifier::Generic>& paymentCodeAccounts) noexcept
    -> std::unique_ptr<blockchain::crypto::Account>;
auto BlockchainHDSubaccount(
    const api::Session& api,
    const blockchain::crypto::Account& parent,
    const proto::HDPath& path,
    const blockchain::crypto::HDProtocol standard,
    const PasswordPrompt& reason,
    identifier::Generic& id) noexcept
    -> std::unique_ptr<blockchain::crypto::HD>;
auto BlockchainHDSubaccount(
    const api::Session& api,
    const blockchain::crypto::Account& parent,
    const proto::HDAccount& serialized,
    identifier::Generic& id) noexcept
    -> std::unique_ptr<blockchain::crypto::HD>;
auto BlockchainNotificationSubaccount(
    const api::Session& api,
    const blockchain::crypto::Account& parent,
    const opentxs::PaymentCode& code,
    const identity::Nym& nym,
    identifier::Generic& id) noexcept
    -> std::unique_ptr<blockchain::crypto::Notification>;
auto BlockchainPCSubaccount(
    const api::Session& api,
    const api::session::Contacts& contacts,
    const blockchain::crypto::Account& parent,
    const opentxs::PaymentCode& local,
    const opentxs::PaymentCode& remote,
    const proto::HDPath& path,
    const Data& txid,
    const PasswordPrompt& reason,
    identifier::Generic& id) noexcept
    -> std::unique_ptr<blockchain::crypto::PaymentCode>;
auto BlockchainPCSubaccount(
    const api::Session& api,
    const api::session::Contacts& contacts,
    const blockchain::crypto::Account& parent,
    const proto::Bip47Channel& serialized,
    identifier::Generic& id) noexcept
    -> std::unique_ptr<blockchain::crypto::PaymentCode>;
auto BlockchainWalletKeys(
    const api::Session& api,
    const api::session::Contacts& contacts,
    const api::crypto::Blockchain& parent,
    const blockchain::crypto::AccountIndex& index,
    const blockchain::Type chain) noexcept
    -> std::unique_ptr<blockchain::crypto::Wallet>;
}  // namespace opentxs::factory
