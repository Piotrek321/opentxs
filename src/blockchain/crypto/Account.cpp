// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                   // IWYU pragma: associated
#include "1_Internal.hpp"                 // IWYU pragma: associated
#include "blockchain/crypto/Account.hpp"  // IWYU pragma: associated

#include <Bip47Channel.pb.h>
#include <HDAccount.pb.h>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "blockchain/crypto/Account.tpp"  // IWYU pragma: keep
#include "blockchain/crypto/AccountIndex.hpp"
#include "internal/blockchain/crypto/Crypto.hpp"
#include "internal/blockchain/crypto/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/api/session/Wallet.hpp"
#include "opentxs/blockchain/crypto/AddressStyle.hpp"
#include "opentxs/blockchain/crypto/Element.hpp"
#include "opentxs/blockchain/crypto/HD.hpp"
#include "opentxs/blockchain/crypto/HDProtocol.hpp"
#include "opentxs/blockchain/crypto/SubaccountType.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Iterator.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/WorkType.hpp"

namespace opentxs::factory
{
auto BlockchainAccountKeys(
    const api::Session& api,
    const api::session::Contacts& contacts,
    const blockchain::crypto::Wallet& parent,
    const blockchain::crypto::AccountIndex& index,
    const identifier::Nym& id,
    const UnallocatedSet<OTIdentifier>& hd,
    const UnallocatedSet<OTIdentifier>& imported,
    const UnallocatedSet<OTIdentifier>& pc) noexcept
    -> std::unique_ptr<blockchain::crypto::Account>
{
    using ReturnType = blockchain::crypto::implementation::Account;

    return std::make_unique<ReturnType>(
        api, contacts, parent, index, id, hd, imported, pc);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::crypto::implementation
{
Account::Account(
    const api::Session& api,
    const api::session::Contacts& contacts,
    const crypto::Wallet& parent,
    const AccountIndex& index,
    const identifier::Nym& nym,
    const Accounts& hd,
    const Accounts& imported,
    const Accounts& paymentCode) noexcept
    : api_(api)
    , contacts_(contacts)
    , parent_(parent)
    , account_index_(index)
    , chain_(parent.Chain())
    , nym_id_(nym)
    , account_id_([&] {
        auto out = api_.Factory().Identifier();
        auto preimage = api_.Factory().DataFromBytes(nym_id_->Bytes());
        const auto chain = parent.Chain();
        preimage.Concatenate(&chain, sizeof(chain));
        out->CalculateDigest(preimage.Bytes());

        return out;
    }())
    , hd_(api_, SubaccountType::HD, *this)
    , imported_(api_, SubaccountType::Imported, *this)
    , notification_(api_, SubaccountType::Notification, *this)
    , payment_code_(api_, SubaccountType::PaymentCode, *this)
    , node_index_()
    , lock_()
    , unspent_()
    , spent_()
    , find_nym_([&] {
        using Dir = network::zeromq::socket::Direction;
        auto out = api_.Network().ZeroMQ().PushSocket(Dir::Connect);
        const auto started = out->Start(api_.Endpoints().FindNym().data());

        OT_ASSERT(started);

        return out;
    }())
{
    init_hd(hd);
    init_payment_code(paymentCode);
}

auto Account::NodeIndex::Add(
    const UnallocatedCString& id,
    crypto::Subaccount* node) noexcept -> void
{
    OT_ASSERT(nullptr != node);

    Lock lock(lock_);
    index_[id] = node;
}

auto Account::NodeIndex::Find(const UnallocatedCString& id) const noexcept
    -> crypto::Subaccount*
{
    Lock lock(lock_);

    try {

        return index_.at(id);
    } catch (...) {

        return nullptr;
    }
}

auto Account::AddHDNode(
    const proto::HDPath& path,
    const crypto::HDProtocol standard,
    const PasswordPrompt& reason,
    Identifier& id) noexcept -> bool
{
    init_notification();

    return hd_.Construct(id, path, standard, reason);
}

auto Account::AssociateTransaction(
    const UnallocatedVector<Activity>& unspent,
    const UnallocatedVector<Activity>& spent,
    UnallocatedSet<OTIdentifier>& contacts,
    const PasswordPrompt& reason) const noexcept -> bool
{
    using ActivityVector = UnallocatedVector<Activity>;
    using ActivityPair = std::pair<ActivityVector, ActivityVector>;
    using ActivityMap = UnallocatedMap<UnallocatedCString, ActivityPair>;

    Lock lock(lock_);
    auto sorted = ActivityMap{};
    auto outputs =
        UnallocatedMap<UnallocatedCString, UnallocatedMap<std::size_t, int>>{};

    for (const auto& [coin, key, amount] : unspent) {
        const auto& [transaction, output] = coin;
        const auto& [account, subchain, index] = key;

        if (1 < ++outputs[transaction][output]) { return false; }
        if (0 >= amount) { return false; }

        sorted[account].first.emplace_back(Activity{coin, key, amount});
    }

    for (const auto& [coin, key, amount] : spent) {
        const auto& [transaction, output] = coin;
        const auto& [account, subchain, index] = key;

        if (1 < ++outputs[transaction][output]) { return false; }
        if (0 >= amount) { return false; }

        sorted[account].second.emplace_back(Activity{coin, key, amount});
    }

    for (const auto& [accountID, value] : sorted) {
        auto* pNode = node_index_.Find(accountID);

        if (nullptr == pNode) {
            LogVerbose()(OT_PRETTY_CLASS())("Account ")(accountID)(" not found")
                .Flush();

            continue;
        }

        const auto& node = *pNode;
        const auto accepted = node.Internal().AssociateTransaction(
            value.first, value.second, contacts, reason);

        if (accepted) {
            for (const auto& [coin, key, amount] : value.first) {
                unspent_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(coin),
                    std::forward_as_tuple(key, amount));
            }

            for (const auto& [coin, key, amount] : value.second) {
                spent_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(coin),
                    std::forward_as_tuple(key, amount));
            }
        } else {
            LogError()(OT_PRETTY_CLASS())("Failed processing transaction")
                .Flush();

            return false;
        }
    }

    return true;
}

auto Account::ClaimAccountID(
    const UnallocatedCString& id,
    crypto::Subaccount* node) const noexcept -> void
{
    node_index_.Add(id, node);
    account_index_.Register(AccountID(), nym_id_, chain_);
}

auto Account::find_next_element(
    Subchain subchain,
    const Identifier& contact,
    const UnallocatedCString& label,
    const PasswordPrompt& reason) const noexcept(false) -> const Element&
{
    // TODO Add a mechanism for setting a default subaccount in case more than
    // one is present. Also handle cases where only an imported subaccount
    // exists

    // Look for a BIP-44 account first
    for (const auto& account : hd_) {
        if (HDProtocol::BIP_44 != account.Standard()) { continue; }

        const auto index = account.Reserve(subchain, reason, contact, label);

        if (index.has_value()) {

            return account.BalanceElement(subchain, index.value());
        }
    }

    // If no BIP-44 account exists, then use whatever else may exist
    for (const auto& account : hd_) {
        const auto index = account.Reserve(subchain, reason, contact, label);

        if (index.has_value()) {

            return account.BalanceElement(subchain, index.value());
        }
    }

    throw std::runtime_error("No available element for selected subchain");
}

auto Account::FindNym(const identifier::Nym& id) const noexcept -> void
{
    auto work = network::zeromq::tagged_message(WorkType::OTXSearchNym);
    work.AddFrame(id);
    find_nym_->Send(std::move(work));
}

auto Account::GetNextChangeKey(const PasswordPrompt& reason) const
    noexcept(false) -> const Element&
{
    static const auto blank = api_.Factory().Identifier();

    return find_next_element(Subchain::Internal, blank, "", reason);
}

auto Account::GetNextDepositKey(const PasswordPrompt& reason) const
    noexcept(false) -> const Element&
{
    static const auto blank = api_.Factory().Identifier();

    return find_next_element(Subchain::External, blank, "", reason);
}

auto Account::GetDepositAddress(
    const blockchain::crypto::AddressStyle style,
    const PasswordPrompt& reason,
    const UnallocatedCString& memo) const noexcept -> UnallocatedCString
{
    static const auto blank = api_.Factory().Identifier();

    return GetDepositAddress(style, blank, reason, memo);
}

auto Account::GetDepositAddress(
    const blockchain::crypto::AddressStyle style,
    const Identifier& contact,
    const PasswordPrompt& reason,
    const UnallocatedCString& memo) const noexcept -> UnallocatedCString
{
    const auto& element =
        find_next_element(Subchain::External, contact, memo, reason);

    return element.Address(style);
}

auto Account::init_hd(const Accounts& accounts) noexcept -> void
{
    for (const auto& accountID : accounts) {
        init_notification();
        auto account = proto::HDAccount{};
        const auto loaded =
            api_.Storage().Load(nym_id_->str(), accountID->str(), account);

        if (false == loaded) { continue; }

        auto notUsed = Identifier::Factory();
        hd_.Construct(notUsed, account);
    }
}

auto Account::init_notification() noexcept -> void
{
    const auto nym = api_.Wallet().Nym(nym_id_);

    OT_ASSERT(nym);

    if (auto code = api_.Factory().PaymentCode(nym->PaymentCode());
        0 < code.Version()) {
        auto notUsed = Identifier::Factory();
        notification_.Construct(notUsed, code, *nym);
    }
}

auto Account::init_payment_code(const Accounts& accounts) noexcept -> void
{
    for (const auto& id : accounts) {
        auto account = proto::Bip47Channel{};
        const auto loaded = api_.Storage().Load(nym_id_, id, account);

        if (false == loaded) { continue; }

        auto notUsed = Identifier::Factory();
        payment_code_.Construct(notUsed, contacts_, account);
    }
}

auto Account::LookupUTXO(const Coin& coin) const noexcept
    -> std::optional<std::pair<Key, Amount>>
{
    Lock lock(lock_);

    try {

        return unspent_.at(coin);
    } catch (...) {

        return {};
    }
}

auto Account::Subaccount(const Identifier& id) const noexcept(false)
    -> const crypto::Subaccount&
{
    auto* output = node_index_.Find(id.str());

    if (nullptr == output) { throw std::out_of_range("Account not found"); }

    return *output;
}
}  // namespace opentxs::blockchain::crypto::implementation
