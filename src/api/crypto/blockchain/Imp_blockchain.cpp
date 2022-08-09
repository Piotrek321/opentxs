// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                              // IWYU pragma: associated
#include "1_Internal.hpp"                            // IWYU pragma: associated
#include "api/crypto/blockchain/Imp_blockchain.hpp"  // IWYU pragma: associated

#include <BlockchainTransaction.pb.h>
#include <StorageThread.pb.h>
#include <StorageThreadItem.pb.h>
#include <algorithm>
#include <iosfwd>
#include <iterator>
#include <sstream>
#include <string_view>
#include <utility>

#include "Proto.hpp"
#include "blockchain/database/common/Database.hpp"
#include "internal/api/crypto/blockchain/BalanceOracle.hpp"
#include "internal/api/network/Blockchain.hpp"
#include "internal/blockchain/bitcoin/block/Transaction.hpp"
#include "internal/blockchain/node/Types.hpp"
#include "internal/network/zeromq/message/Message.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/P0330.hpp"
#include "opentxs/api/crypto/Hash.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Activity.hpp"
#include "opentxs/api/session/Contacts.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/crypto/SubaccountType.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/crypto/HashType.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ZeroMQ.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Sender.hpp"  // IWYU pragma: keep
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Container.hpp"
#include "util/Work.hpp"

namespace opentxs::api::crypto::imp
{
BlockchainImp::BlockchainImp(
    const api::Session& api,
    const api::session::Activity& activity,
    const api::session::Contacts& contacts,
    const api::Legacy& legacy,
    const UnallocatedCString& dataFolder,
    const Options& args,
    api::crypto::Blockchain& parent) noexcept
    : Imp(api, contacts, parent)
    , activity_(activity)
    , key_generated_endpoint_(opentxs::network::zeromq::MakeArbitraryInproc())
    , transaction_updates_([&] {
        auto out = api_.Network().ZeroMQ().PublishSocket();
        const auto listen =
            out->Start(api_.Endpoints().BlockchainTransactions().data());

        OT_ASSERT(listen);

        return out;
    }())
    , key_updates_([&] {
        auto out = api_.Network().ZeroMQ().PublishSocket();
        const auto listen = out->Start(key_generated_endpoint_);

        OT_ASSERT(listen);

        return out;
    }())
    , scan_updates_([&] {
        auto out = api_.Network().ZeroMQ().PublishSocket();
        const auto listen =
            out->Start(api_.Endpoints().BlockchainScanProgress().data());

        OT_ASSERT(listen);

        return out;
    }())
    , new_blockchain_accounts_([&] {
        auto out = api_.Network().ZeroMQ().PublishSocket();
        const auto listen =
            out->Start(api_.Endpoints().BlockchainAccountCreated().data());

        OT_ASSERT(listen);

        return out;
    }())
{
}

auto BlockchainImp::ActivityDescription(
    const identifier::Nym& nym,
    const identifier::Generic& thread,
    const UnallocatedCString& itemID) const noexcept -> UnallocatedCString
{
    auto data = proto::StorageThread{};

    if (false ==
        api_.Storage().Load(nym, thread.asBase58(api_.Crypto()), data)) {
        LogError()(OT_PRETTY_CLASS())("thread ")(
            thread)(" does not exist for nym ")(nym)
            .Flush();

        return {};
    }

    for (const auto& item : data.item()) {
        if (item.id() != itemID) { continue; }

        const auto txid = api_.Factory().DataFromBytes(item.txid());
        const auto chain = static_cast<opentxs::blockchain::Type>(item.chain());
        const auto pTx = LoadTransactionBitcoin(txid);

        if (false == bool(pTx)) {
            LogError()(OT_PRETTY_CLASS())("failed to load transaction ")
                .asHex(txid)
                .Flush();

            return {};
        }

        const auto& tx = *pTx;

        return this->ActivityDescription(nym, chain, tx);
    }

    LogError()(OT_PRETTY_CLASS())("item ")(itemID)(" not found ").Flush();

    return {};
}

auto BlockchainImp::ActivityDescription(
    const identifier::Nym& nym,
    const opentxs::blockchain::Type chain,
    const opentxs::blockchain::bitcoin::block::Transaction& tx) const noexcept
    -> UnallocatedCString
{
    auto output = std::stringstream{};
    const auto amount = tx.NetBalanceChange(nym);
    const auto memo = tx.Memo();
    const auto& contacts = tx.AssociatedRemoteContacts(contacts_, nym);
    std::set<std::string> names;

    for (const auto& id : contacts) {
        names.emplace(contacts_.ContactName(id, BlockchainToUnit(chain)));
    }

    if (0 < amount) {
        output << "Incoming ";
    } else if (0 > amount) {
        output << "Outgoing ";
    }

    output << print(chain);
    output << " transaction";

    if (0 < names.size()) {
        output << " ";

        if (0 < amount) {
            output << "from ";
        } else {
            output << "to ";
        }

        auto n = 0_uz;
        const auto max = names.size();

        for (auto i = names.begin(); i != names.end(); ++n, ++i) {
            if (0_uz == n) {
                output << *i;
            } else if ((n + 1_uz) == max) {
                output << ", and ";
                output << *i;
            } else {
                output << ", ";
                output << *i;
            }
        }
    }

    if (false == memo.empty()) { output << ": " << memo; }

    return output.str();
}

auto BlockchainImp::AssignTransactionMemo(
    const TxidHex& id,
    const UnallocatedCString& label) const noexcept -> bool
{
    auto lock = Lock{lock_};
    auto proto = proto::BlockchainTransaction{};
    auto pTransaction = load_transaction(lock, id, proto);

    if (false == bool(pTransaction)) {
        LogError()(OT_PRETTY_CLASS())("transaction ")(label)(" does not exist")
            .Flush();

        return false;
    }

    auto& transaction = pTransaction->Internal();
    transaction.SetMemo(label);
    const auto& db = api_.Network().Blockchain().Internal().Database();

    if (false == db.StoreTransaction(transaction)) {
        LogError()(OT_PRETTY_CLASS())("failed to save updated transaction ")(id)
            .Flush();

        return false;
    }

    broadcast_update_signal(proto, transaction);

    return true;
}

auto BlockchainImp::broadcast_update_signal(
    const UnallocatedVector<pTxid>& transactions) const noexcept -> void
{
    const auto& db = api_.Network().Blockchain().Internal().Database();
    std::for_each(
        std::begin(transactions),
        std::end(transactions),
        [this, &db](const auto& txid) {
            auto proto = proto::BlockchainTransaction{};
            const auto tx = db.LoadTransaction(txid.Bytes(), proto);

            OT_ASSERT(tx);

            broadcast_update_signal(proto, *tx);
        });
}

auto BlockchainImp::broadcast_update_signal(
    const proto::BlockchainTransaction& proto,
    const opentxs::blockchain::bitcoin::block::Transaction& tx) const noexcept
    -> void
{
    const auto chains = tx.Chains();
    std::for_each(std::begin(chains), std::end(chains), [&](const auto& chain) {
        auto work = opentxs::network::zeromq::tagged_message(
            WorkType::BlockchainNewTransaction);
        work.AddFrame(tx.ID());
        work.AddFrame(chain);
        work.Internal().AddFrame(proto);

        transaction_updates_->Send(std::move(work));
    });
}

auto BlockchainImp::IndexItem(const ReadView bytes) const noexcept -> PatternID
{
    auto output = PatternID{};
    const auto hashed = api_.Crypto().Hash().HMAC(
        opentxs::crypto::HashType::SipHash24,
        api_.Network().Blockchain().Internal().Database().HashKey(),
        bytes,
        preallocated(sizeof(output), &output));

    OT_ASSERT(hashed);

    return output;
}

auto BlockchainImp::KeyEndpoint() const noexcept -> std::string_view
{
    return key_generated_endpoint_;
}

auto BlockchainImp::KeyGenerated(
    const opentxs::blockchain::Type chain,
    const identifier::Nym& account,
    const identifier::Generic& subaccount,
    const opentxs::blockchain::crypto::SubaccountType type,
    const opentxs::blockchain::crypto::Subchain subchain) const noexcept -> void
{
    auto work = MakeWork(OT_ZMQ_NEW_BLOCKCHAIN_WALLET_KEY_SIGNAL);
    work.AddFrame(chain);
    work.AddFrame(account);
    work.AddFrame(subaccount);
    work.AddFrame(subchain);
    work.AddFrame(type);

    key_updates_->Send(std::move(work));
}

auto BlockchainImp::LoadTransactionBitcoin(const TxidHex& txid) const noexcept
    -> std::unique_ptr<const opentxs::blockchain::bitcoin::block::Transaction>
{
    auto lock = Lock{lock_};

    return load_transaction(lock, txid);
}

auto BlockchainImp::LoadTransactionBitcoin(const Txid& txid) const noexcept
    -> std::unique_ptr<const opentxs::blockchain::bitcoin::block::Transaction>
{
    auto lock = Lock{lock_};

    return load_transaction(lock, txid);
}

auto BlockchainImp::load_transaction(const Lock& lock, const TxidHex& txid)
    const noexcept
    -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Transaction>
{
    auto proto = proto::BlockchainTransaction{};

    return load_transaction(lock, txid, proto);  // TODO: MT-80
}

auto BlockchainImp::load_transaction(
    const Lock& lock,
    const TxidHex& txid,
    proto::BlockchainTransaction& out) const noexcept
    -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Transaction>
{
    return load_transaction(lock, api_.Factory().DataFromHex(txid), out);
}

auto BlockchainImp::load_transaction(const Lock& lock, const Txid& txid)
    const noexcept
    -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Transaction>
{
    auto proto = proto::BlockchainTransaction{};

    return load_transaction(lock, txid, proto);  // TODO: MT-80
}

auto BlockchainImp::load_transaction(
    const Lock& lock,
    const Txid& txid,
    proto::BlockchainTransaction& out) const noexcept
    -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Transaction>
{
    return api_.Network().Blockchain().Internal().Database().LoadTransaction(
        txid.Bytes(), out);
}

auto BlockchainImp::LookupContacts(const Data& pubkeyHash) const noexcept
    -> ContactList
{
    return api_.Network().Blockchain().Internal().Database().LookupContact(
        pubkeyHash);
}

auto BlockchainImp::notify_new_account(
    const identifier::Generic& id,
    const identifier::Nym& owner,
    opentxs::blockchain::Type chain,
    opentxs::blockchain::crypto::SubaccountType type) const noexcept -> void
{
    auto work = opentxs::network::zeromq::tagged_message(
        WorkType::BlockchainAccountCreated);
    work.AddFrame(chain);
    work.AddFrame(owner);
    work.AddFrame(type);
    work.AddFrame(id);

    new_blockchain_accounts_->Send(std::move(work));
}

auto BlockchainImp::ProcessContact(const Contact& contact) const noexcept
    -> bool
{
    broadcast_update_signal(
        api_.Network().Blockchain().Internal().Database().UpdateContact(
            contact));

    return true;
}

auto BlockchainImp::ProcessMergedContact(
    const Contact& parent,
    const Contact& child) const noexcept -> bool
{
    broadcast_update_signal(
        api_.Network().Blockchain().Internal().Database().UpdateMergedContact(
            parent, child));

    return true;
}

auto BlockchainImp::ProcessTransactions(
    const opentxs::blockchain::Type chain,
    Set<std::shared_ptr<opentxs::blockchain::bitcoin::block::Transaction>>&& in,
    const PasswordPrompt& reason) const noexcept -> bool
{
    const auto& db = api_.Network().Blockchain().Internal().Database();
    const auto& log = LogTrace();
    auto lock = Lock{lock_};

    for (const auto& pTX : in) {  // TODO: MT-82
        OT_ASSERT(pTX);           // TODO: MT-81

        const auto& tx = *pTX;
        const auto& id = tx.ID();
        const auto txid = id.Bytes();
        auto old = db.LoadTransaction(txid);
        auto proto = proto::BlockchainTransaction{};

        if (old) {
            old->Internal().MergeMetadata(chain, tx.Internal(), log);

            if (false == db.StoreTransaction(*old, proto)) {
                LogError()(OT_PRETTY_CLASS())(
                    "failed to save updated transaction ")(id.asHex())
                    .Flush();

                return false;
            }
        } else {
            if (false == db.StoreTransaction(tx, proto)) {
                LogError()(OT_PRETTY_CLASS())(
                    "failed to save new transaction ")(id.asHex())
                    .Flush();

                return false;
            }
        }

        if (false == db.AssociateTransaction(id, tx.Internal().GetPatterns())) {
            LogError()(OT_PRETTY_CLASS())(
                "failed to associate patterns for transaction ")(id.asHex())
                .Flush();

            return false;
        }

        if (!reconcile_activity_threads(lock, proto, (old ? *old : tx))) {

            return false;
        }
    }

    return true;
}

auto BlockchainImp::reconcile_activity_threads(
    const Lock& lock,
    const Txid& txid) const noexcept -> bool
{
    auto proto = proto::BlockchainTransaction{};
    const auto tx = load_transaction(lock, txid, proto);

    if (false == bool(tx)) { return false; }

    return reconcile_activity_threads(lock, proto, *tx);
}

auto BlockchainImp::reconcile_activity_threads(
    const Lock& lock,
    const proto::BlockchainTransaction& proto,
    const opentxs::blockchain::bitcoin::block::Transaction& tx) const noexcept
    -> bool
{
    if (!activity_.AddBlockchainTransaction(tx)) { return false; }

    broadcast_update_signal(proto, tx);

    return true;
}

auto BlockchainImp::ReportScan(
    const opentxs::blockchain::Type chain,
    const identifier::Nym& owner,
    const opentxs::blockchain::crypto::SubaccountType type,
    const identifier::Generic& id,
    const Blockchain::Subchain subchain,
    const opentxs::blockchain::block::Position& progress) const noexcept -> void
{
    OT_ASSERT(false == owner.empty());
    OT_ASSERT(false == id.empty());

    const auto bytes = id.Bytes();
    const auto hash = progress.hash_.Bytes();
    auto work = opentxs::network::zeromq::tagged_message(
        WorkType::BlockchainWalletScanProgress);
    work.AddFrame(chain);
    work.AddFrame(owner.data(), owner.size());
    work.AddFrame(type);
    work.AddFrame(bytes.data(), bytes.size());
    work.AddFrame(subchain);
    work.AddFrame(progress.height_);
    work.AddFrame(hash.data(), hash.size());

    scan_updates_->Send(std::move(work));
}

auto BlockchainImp::Start(std::shared_ptr<const api::Session> api) noexcept
    -> void
{
    blockchain::BalanceOracle{std::move(api), balance_oracle_endpoint_}.Start();
}

auto BlockchainImp::Unconfirm(
    const Blockchain::Key key,
    const opentxs::blockchain::block::Txid& txid,
    const Time time) const noexcept -> bool
{
    auto out = Imp::Unconfirm(key, txid, time);
    auto lock = Lock{lock_};

    if (auto tx = load_transaction(lock, txid); tx) {
        static const auto null =
            make_blank<opentxs::blockchain::block::Position>::value(api_);
        tx->Internal().SetMinedPosition(null);
        const auto& db = api_.Network().Blockchain().Internal().Database();

        if (false == db.StoreTransaction(*tx)) {
            LogError()(OT_PRETTY_CLASS())(
                "failed to save updated transaction ")(txid.asHex())
                .Flush();

            return false;
        }
    }

    return out;
}

auto BlockchainImp::UpdateElement(
    UnallocatedVector<ReadView>& hashes) const noexcept -> void
{
    auto patterns = UnallocatedVector<PatternID>{};
    std::for_each(std::begin(hashes), std::end(hashes), [&](const auto& bytes) {
        patterns.emplace_back(IndexItem(bytes));
    });
    LogTrace()(OT_PRETTY_CLASS())(patterns.size())(
        " pubkey hashes have changed:")
        .Flush();
    auto transactions = UnallocatedVector<pTxid>{};
    std::for_each(
        std::begin(patterns), std::end(patterns), [&](const auto& pattern) {
            LogTrace()("    * ")(pattern).Flush();
            auto matches = api_.Network()
                               .Blockchain()
                               .Internal()
                               .Database()
                               .LookupTransactions(pattern);
            transactions.reserve(transactions.size() + matches.size());
            std::move(
                std::begin(matches),
                std::end(matches),
                std::back_inserter(transactions));
        });
    dedup(transactions);
    auto lock = Lock{lock_};
    std::for_each(
        std::begin(transactions),
        std::end(transactions),
        [&](const auto& txid) { reconcile_activity_threads(lock, txid); });
}
}  // namespace opentxs::api::crypto::imp
