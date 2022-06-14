// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/wallet/subchain/NotificationStateData.hpp"  // IWYU pragma: associated

#include <boost/smart_ptr/shared_ptr.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "internal/api/crypto/Blockchain.hpp"
#include "internal/api/session/Session.hpp"
#include "internal/blockchain/crypto/Crypto.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/core/PaymentCode.hpp"
#include "internal/util/BoostPMR.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/session/Contacts.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/api/session/Wallet.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/block/Output.hpp"
#include "opentxs/blockchain/bitcoin/block/Outputs.hpp"
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/crypto/Notification.hpp"
#include "opentxs/blockchain/crypto/PaymentCode.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"  // IWYU pragma: keep
#include "opentxs/core/Contact.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/crypto/key/HD.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/NymEditor.hpp"

namespace opentxs::blockchain::node::wallet
{
NotificationStateData::NotificationStateData(
    const api::Session& api,
    const node::internal::Manager& node,
    database::Wallet& db,
    const node::internal::Mempool& mempool,
    const cfilter::Type filter,
    const crypto::Subchain subchain,
    const network::zeromq::BatchID batch,
    const std::string_view parent,
    const opentxs::PaymentCode& code,
    const crypto::Notification& subaccount,
    allocator_type alloc) noexcept
    : SubchainStateData(
          api,
          node,
          db,
          mempool,
          subaccount,
          filter,
          subchain,
          batch,
          parent,
          std::move(alloc))
    , path_(subaccount.InternalNotification().Path())
    , pc_display_(code.asBase58(), get_allocator())
    , code_(code)
    , cache_(get_allocator())
{
}

auto NotificationStateData::CheckCache(const std::size_t, FinishedCallback cb)
    const noexcept -> void
{
    if (cb) {
        cache_.modify([&](auto& data) {
            cb(data);
            data.clear();
        });
    }
}

auto NotificationStateData::do_startup() noexcept -> void
{
    SubchainStateData::do_startup();
    auto reason =
        api_.Factory().PasswordPrompt("Verifying / updating contact data");
    auto mNym = api_.Wallet().mutable_Nym(owner_, reason);
    const auto type = BlockchainToUnit(chain_);
    const auto existing = mNym.PaymentCode(type);
    const auto expected = UnallocatedCString{pc_display_};

    if (existing != expected) {
        mNym.AddPaymentCode(expected, type, existing.empty(), true, reason);
    }
}

auto NotificationStateData::get_index(
    const boost::shared_ptr<const SubchainStateData>& me) const noexcept
    -> Index
{
    return Index::NotificationFactory(me, *code_.lock_shared());
}

auto NotificationStateData::handle_confirmed_matches(
    const bitcoin::block::Block& block,
    const block::Position& position,
    const block::Matches& confirmed,
    const Log& log) const noexcept -> void
{
    const auto& [utxo, general] = confirmed;
    log(OT_PRETTY_CLASS())(general.size())(" confirmed matches for ")(
        pc_display_)(" on ")(print(node_.Chain()))
        .Flush();

    if (general.empty()) { return; }

    const auto reason = init_keys();

    for (const auto& match : general) {
        const auto& [txid, elementID] = match;
        const auto& [version, subchainID] = elementID;
        log(OT_PRETTY_CLASS())(print(node_.Chain()))(" transaction ")
            .asHex(txid)(" contains a version ")(version)(" notification for ")(
                pc_display_)
            .Flush();
        const auto& tx = block.at(txid->Bytes());

        OT_ASSERT(tx);

        process(match, *tx, reason);
    }

    cache_.modify([&](auto& vector) { vector.emplace_back(position); });
}

auto NotificationStateData::handle_mempool_matches(
    const block::Matches& matches,
    std::unique_ptr<const bitcoin::block::Transaction> tx) const noexcept
    -> void
{
    const auto& [utxo, general] = matches;

    if (general.empty()) { return; }

    const auto reason = init_keys();

    for (const auto& match : general) {
        const auto& [txid, elementID] = match;
        const auto& [version, subchainID] = elementID;
        log_(OT_PRETTY_CLASS())(print(node_.Chain()))(" mempool transaction ")(
            txid->asHex())(" contains a version ")(
            version)(" notification for ")(pc_display_)
            .Flush();
        process(match, *tx, reason);
    }
}

auto NotificationStateData::init_contacts() noexcept -> void
{
    auto buf = std::array<std::byte, 4096>{};
    auto alloc = alloc::BoostMonotonic{buf.data(), buf.size()};
    const auto& api = api_.Internal().Contacts();
    Vector<OTIdentifier> contacts{&alloc};
    const auto data = api.ContactList();
    std::transform(
        data.begin(),
        data.end(),
        std::back_inserter(contacts),
        [this](const auto& item) {
            return api_.Factory().Identifier(item.first);
        });

    for (const auto& id : contacts) {
        const auto contact = api.Contact(id);

        OT_ASSERT(contact);

        for (const auto& remote : contact->PaymentCodes(&alloc)) {
            // TODO use allocator when we upgrade to c++20
            std::stringstream prompt{};
            prompt << "Generate keys for a ";
            prompt << print(node_.Chain());
            prompt << " payment code account for ";
            prompt << api.ContactName(id);

            const auto reason = api_.Factory().PasswordPrompt(prompt.str());
            process(remote, reason);
        }
    }
}

auto NotificationStateData::init_keys() const noexcept -> OTPasswordPrompt
{
    auto reason = api_.Factory().PasswordPrompt(
        "Decoding payment code notification transaction");
    auto handle = code_.lock();

    if (auto key{handle->Key()}; key) {
        if (!key->HasPrivate()) {
            auto seed{path_.root()};
            const auto upgraded = handle->Internal().AddPrivateKeys(
                seed, *path_.child().rbegin(), reason);

            if (!upgraded) { OT_FAIL; }
        }
    } else {
        OT_FAIL;
    }

    return reason;
}

auto NotificationStateData::process(
    const block::Match& match,
    const bitcoin::block::Transaction& tx,
    const PasswordPrompt& reason) const noexcept -> void
{
    const auto& [txid, elementID] = match;
    const auto& [version, subchainID] = elementID;
    auto handle = code_.lock_shared();

    for (const auto& output : tx.Outputs()) {
        const auto& script = output.Script();

        if (script.IsNotification(version, *handle)) {
            UnallocatedVector<Space> elements{};

            for (auto i{0u}; i < 3u; ++i) {
                const auto view = script.MultisigPubkey(i);

                OT_ASSERT(view.has_value());

                const auto& value = view.value();
                auto* start = reinterpret_cast<const std::byte*>(value.data());
                auto* stop = std::next(start, value.size());
                elements.emplace_back(start, stop);
            }

            auto sender =
                handle->DecodeNotificationElements(version, elements, reason);

            if (0u == sender.Version()) { continue; }

            log_(OT_PRETTY_CLASS())("decoded incoming notification from ")(
                sender.asBase58())(" on ")(print(node_.Chain()))(" for ")(
                pc_display_)
                .Flush();
            process(sender, reason);
        }
    }
}

auto NotificationStateData::process(
    const opentxs::PaymentCode& remote,
    const PasswordPrompt& reason) const noexcept -> void
{
    auto handle = code_.lock_shared();

    if (remote == *handle) { return; }

    const auto& account =
        api_.Crypto().Blockchain().Internal().PaymentCodeSubaccount(
            owner_, *handle, remote, path_, node_.Chain(), reason);
    log_(OT_PRETTY_CLASS())("Created or verified account ")(account.ID())(
        " for ")(remote.asBase58())
        .Flush();
}

auto NotificationStateData::work() noexcept -> int
{
    auto again = SubchainStateData::work();
    init_contacts();
    return again;
}
}  // namespace opentxs::blockchain::node::wallet
