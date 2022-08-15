// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "interface/ui/accountactivity/BlockchainAccountActivity.hpp"  // IWYU pragma: associated

#include <opentxs/util/Log.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>

#include "Proto.tpp"
#include "interface/ui/base/List.hpp"
#include "interface/ui/base/Widget.hpp"
#include "internal/api/crypto/blockchain/Types.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/Params.hpp"
#include "internal/blockchain/bitcoin/block/Factory.hpp"
#include "internal/blockchain/bitcoin/block/Transaction.hpp"
#include "internal/blockchain/crypto/Crypto.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/core/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/crypto/Account.hpp"
#include "opentxs/blockchain/crypto/AddressStyle.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/core/AccountType.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/PaymentCode.hpp"
#include "opentxs/core/display/Definition.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "serialization/protobuf/BlockchainTransaction.pb.h"
#include "serialization/protobuf/PaymentEvent.pb.h"
#include "serialization/protobuf/PaymentWorkflow.pb.h"
#include "serialization/protobuf/PaymentWorkflowEnums.pb.h"
#include "util/Container.hpp"

namespace opentxs::factory
{
auto BlockchainAccountActivityModel(
    const api::session::Client& api,
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const SimpleCallback& cb) noexcept
    -> std::unique_ptr<ui::internal::AccountActivity>
{
    using ReturnType = ui::implementation::BlockchainAccountActivity;
    const auto [chain, owner] =
        api.Crypto().Blockchain().LookupAccount(accountID);

    OT_ASSERT(owner == nymID);

    return std::make_unique<ReturnType>(api, chain, nymID, accountID, cb);
}
}  // namespace opentxs::factory

namespace opentxs::ui::implementation
{
BlockchainAccountActivity::BlockchainAccountActivity(
    const api::session::Client& api,
    const blockchain::Type chain,
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const SimpleCallback& cb) noexcept
    : AccountActivity(api, nymID, accountID, AccountType::Blockchain, cb)
    , chain_(chain)
    , confirmed_(0)
    , balance_cb_(network::zeromq::ListenCallback::Factory(
          [this](auto&& in) { pipeline_.Push(std::move(in)); }))
    , balance_socket_(Widget::api_.Network().ZeroMQ().DealerSocket(
          balance_cb_,
          network::zeromq::socket::Direction::Connect))
    , progress_()
    , height_(0)
    , last_job_{}
    , transactions_{}
    , active_{}
    , iload_{transactions_.cend()}
    , eload_{transactions_.cend()}
    , load_in_progress_{}
    , load_lock_{}
{
    const auto connected = balance_socket_->Start(
        Widget::api_.Endpoints().BlockchainBalance().data());

    OT_ASSERT(connected);

    start();
    init({
        UnallocatedCString{api.Endpoints().BlockchainReorg()},
        UnallocatedCString{api.Endpoints().BlockchainStateChange()},
        UnallocatedCString{api.Endpoints().BlockchainSyncProgress()},
        UnallocatedCString{api.Endpoints().BlockchainTransactions()},
        UnallocatedCString{api.Endpoints().BlockchainTransactions(nymID)},
        UnallocatedCString{api.Endpoints().ContactUpdate()},
    });
    balance_socket_->Send([&] {
        using Job = api::crypto::blockchain::BalanceOracleJobs;
        auto work = network::zeromq::tagged_message(Job::registration);
        work.AddFrame(chain_);
        work.AddFrame(nymID);

        return work;
    }());
}

auto BlockchainAccountActivity::DepositAddress(
    const blockchain::Type chain) const noexcept -> UnallocatedCString
{
    if ((blockchain::Type::Unknown != chain) && (chain_ != chain)) {
        return {};
    }

    const auto& wallet =
        Widget::api_.Crypto().Blockchain().Account(primary_id_, chain_);
    const auto reason = Widget::api_.Factory().PasswordPrompt(
        "Calculating next deposit address");
    const auto& styles = blockchain::params::Chains().at(chain_).styles_;

    if (0u == styles.size()) { return {}; }

    return wallet.GetDepositAddress(styles.front().first, reason);
}

auto BlockchainAccountActivity::display_balance(
    opentxs::Amount value) const noexcept -> UnallocatedCString
{
    return blockchain::internal::Format(chain_, value);
}

auto BlockchainAccountActivity::load_thread() noexcept -> void
{
    bool go{};
    {
        std::unique_lock<std::mutex> L(load_lock_);
        if (load_in_progress_) {
            tdiag("ZZZZ load_thread load_in_progress_");
            return;
        }
        tdiag("ZZZZ load_thread");
        load_in_progress_ = true;
        transactions_ = [&]() -> UnallocatedVector<blockchain::block::pTxid> {
            try {
                auto& chain =
                    Widget::api_.Network().Blockchain().GetChain(chain_);
                height_ = chain.HeaderOracle().BestChain().first;

                return chain.Internal().GetTransactions(primary_id_);
            } catch (...) {
                tdiag("ZZZZ load_thread transactions exception");
                return {};
            }
        }();
        iload_ = transactions_.cbegin();
        eload_ = transactions_.cend();
        active_.clear();
        go = iload_ != eload_;
    }

    if (go) {
        tdiag("ZZZZ load_thread start");
        trigger();
    } else {
        load_in_progress_ = false;
    }
}

auto BlockchainAccountActivity::load_thread_portion() noexcept -> bool
{
    using namespace std::chrono;
    static thread_local auto cc = 0ull;
    int ct = 0;
    std::unique_lock<std::mutex> L(load_lock_);
    tdiag("ZZZZ load_thread_portion, about to process");
    auto Tstart = Clock::now();
    while (iload_ != eload_) {
        auto txid = *iload_;
        ++iload_;
        ++cc;
        if (const auto id = process_txid(txid); id.has_value()) {
            tdiag("ZZZZ load_thread_portion 1");
            auto t0 = Clock::now();
            active_.emplace(id.value());
            auto t1 = Clock::now();
            tdiag(
                "ZZZZ load_thread_portion 2, added after [us]: ",
                (duration_cast<microseconds>(t1 - t0)).count());
        }
        using namespace std::literals;
        if (++ct == 50) {
            ct = 0;
            if (Clock::now() - Tstart > 200ms) { break; }
        }
    }
    tdiag("ZZZZ load_thread_portion, processed in total: ", cc);
    if (iload_ == eload_) {
        cc = 0;
        load_in_progress_ = false;
        transactions_.clear();
        iload_ = transactions_.cbegin();
        eload_ = transactions_.cend();
        auto T1 = Clock::now();
        delete_inactive(active_);
        auto T2 = Clock::now();
        tdiag(
            "ZZZZ load_thread_portion, deleted inactive in[us]:",
            (duration_cast<microseconds>(T2 - T1)).count());
    }
    return load_in_progress_;
}

auto BlockchainAccountActivity::load_thread_old() noexcept -> void
{
    const auto transactions =
        [&]() -> UnallocatedVector<blockchain::block::pTxid> {
        try {
            const auto& chain =
                Widget::api_.Network().Blockchain().GetChain(chain_);
            height_ = chain.HeaderOracle().BestChain().first;

            return chain.Internal().GetTransactions(primary_id_);
        } catch (...) {

            return {};
        }
    }();
    auto active = UnallocatedSet<AccountActivityRowID>{};
    for (const auto& txid : transactions) {
        tdiag("ZZZZ load_thread 1, size = ", transactions.size());
        if (const auto id = process_txid(txid); id.has_value()) {
            active.emplace(id.value());
        }
        tdiag("ZZZZ load_thread 2, size = ", transactions.size());
    }
    delete_inactive(active);
}

auto BlockchainAccountActivity::pipeline(Message&& in) noexcept -> void
{
    if (false == running_.load()) { return; }

    const auto body = in.Body();

    if (1 > body.size()) {
        LogError()(OT_PRETTY_CLASS())("Invalid message").Flush();

        OT_FAIL;
    }

    const auto work = [&] {
        try {

            return body.at(0).as<Work>();
        } catch (...) {

            OT_FAIL;
        }
    }();
    last_job_ = work;
    tdiag("ZZZZ pipeline Work:", print(work));

    switch (work) {
        case Work::shutdown: {
            protect_shutdown([this] { shut_down(); });
        } break;
        case Work::contact: {
            process_contact(in);
        } break;
        case Work::balance: {
            process_balance(in);
        } break;
        case Work::new_block: {
            process_block(in);
        } break;
        case Work::txid: {
            process_txid(in);
        } break;
        case Work::reorg: {
            process_reorg(in);
        } break;
        case Work::statechange: {
            process_state(in);
        } break;
        case Work::sync: {
            process_sync(in);
        } break;
        case Work::init: {
            startup();
            finish_startup();
        } break;
        case Work::statemachine: {
            do_work();
        } break;
        default: {
            LogError()(OT_PRETTY_CLASS())("Unhandled type").Flush();

            OT_FAIL;
        }
    }
}

auto BlockchainAccountActivity::state_machine() noexcept -> int
{

    return load_thread_portion() ? 300 : -1;
}

auto BlockchainAccountActivity::shut_down() noexcept -> void
{
    close_pipeline();
    // TODO MT-34 investigate what other actions might be needed
}

auto BlockchainAccountActivity::print(Work type) noexcept -> const char*
{
    static const auto map = Map<Work, const char*>{
        {Work::shutdown, "shutdown"},
        {Work::contact, "contact"},
        {Work::balance, "balance"},
        {Work::new_block, "new_block"},
        {Work::txid, "txid"},
        {Work::reorg, "reorg"},
        {Work::statechange, "statechange"},
        {Work::sync, "sync"},
        {Work::init, "init"},
        {Work::statemachine, "statemachine"},
    };

    return map.at(type);
}

auto BlockchainAccountActivity::process_balance(const Message& in) noexcept
    -> void
{
    wait_for_startup();
    const auto body = in.Body();
    OT_ASSERT(4 < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();
    const auto confirmed = factory::Amount(body.at(2));
    const auto unconfirmed = factory::Amount(body.at(3));
    const auto nym = Widget::api_.Factory().NymID(body.at(4));

    LogConsole()(
        "BlockchainAccountActivity::process_balance confirmed balance: " +
        display_balance(confirmed) + ", unconfirmed balance: " +
        display_balance(unconfirmed) + ", nym: " + nym->asHex())
        .Flush();

    OT_ASSERT(chain_ == chain);
    OT_ASSERT(primary_id_ == nym);

    const auto oldBalance = [&] {
        eLock lock(shared_lock_);

        const auto oldbalance = balance_;
        balance_ = unconfirmed;
        return oldbalance;
    }();
    const auto oldConfirmed = [&] {
        eLock lock(shared_lock_);

        const auto oldconfirmed = confirmed_;
        confirmed_ = confirmed;
        return oldconfirmed;
    }();

    if (oldBalance != unconfirmed) {
        notify_balance(unconfirmed);
    } else if (oldConfirmed != confirmed) {
        UpdateNotify();
    }

    tdiag("ZZZZ process_balance");
    load_thread();
}

auto BlockchainAccountActivity::process_block(const Message& in) noexcept
    -> void
{
    const auto body = in.Body();

    OT_ASSERT(3 < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain != chain_) { return; }

    process_height(body.at(3).as<blockchain::block::Height>());
}

auto BlockchainAccountActivity::process_contact(const Message& in) noexcept
    -> void
{
    wait_for_startup();
    const auto body = in.Body();

    OT_ASSERT(1 < body.size());

    const auto contactID = Widget::api_.Factory().Identifier(body.at(1))->str();
    const auto txids = [&] {
        auto out = UnallocatedSet<OTData>{};
        for_each_row([&](const auto& row) {
            for (const auto& id : row.Contacts()) {
                if (contactID == id) {
                    out.emplace(
                        blockchain::NumberToHash(Widget::api_, row.UUID()));

                    break;
                }
            }
        });

        return out;
    }();

    for (const auto& txid : txids) { process_txid(txid); }
}

auto BlockchainAccountActivity::process_height(
    const blockchain::block::Height height) noexcept -> void
{
    if (height == height_) { return; }

    height_ = height;
    tdiag("ZZZZ process_height");
    load_thread();
}

auto BlockchainAccountActivity::process_reorg(const Message& in) noexcept
    -> void
{
    const auto body = in.Body();

    OT_ASSERT(5 < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain != chain_) { return; }

    process_height(body.at(5).as<blockchain::block::Height>());
}

auto BlockchainAccountActivity::process_state(const Message& in) noexcept
    -> void
{
    {
        const auto body = in.Body();

        OT_ASSERT(2 < body.size());

        const auto chain = body.at(1).as<blockchain::Type>();

        if (chain_ != chain) { return; }

        const auto enabled = body.at(2).as<bool>();

        if (false == enabled) { return; }
    }

    try {
        const auto& chain =
            Widget::api_.Network().Blockchain().GetChain(chain_);
        process_height(chain.HeaderOracle().BestChain().first);
    } catch (...) {
    }
}

auto BlockchainAccountActivity::process_sync(const Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(3 < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain != chain_) { return; }

    const auto height = body.at(2).as<blockchain::block::Height>();
    const auto target = body.at(3).as<blockchain::block::Height>();

    OT_ASSERT(height <= std::numeric_limits<int>::max());
    OT_ASSERT(target <= std::numeric_limits<int>::max());

    const auto previous = progress_.get_progress();
    const auto current = static_cast<int>(height);
    const auto max = static_cast<int>(target);
    const auto percent = progress_.set(current, max);

    if (progress_.get_progress() != previous) {
        auto lock = Lock{callbacks_.lock_};
        const auto& cb = callbacks_.cb_.sync_;

        if (cb) { cb(current, max, percent); }

        UpdateNotify();
    }
}

auto BlockchainAccountActivity::process_txid(const Message& in) noexcept -> void
{
    wait_for_startup();
    const auto body = in.Body();

    OT_ASSERT(3 < body.size());

    const auto& api = Widget::api_;
    const auto txid = api.Factory().Data(body.at(1));
    const auto chain = body.at(2).as<blockchain::Type>();

    if (chain != chain_) { return; }

    const auto proto = proto::Factory<proto::BlockchainTransaction>(body.at(3));
    process_txid(txid, factory::BitcoinTransaction(api, proto));
}

auto BlockchainAccountActivity::process_txid(const Data& txid) noexcept
    -> std::optional<AccountActivityRowID>
{
    tdiag("ZZZZ process_txid 0");
    auto t0 = Clock::now();
    auto bct = Widget::api_.Crypto().Blockchain().LoadTransactionBitcoin(txid);
    auto t1 = Clock::now();
    using namespace std::chrono;
    tdiag(
        "ZZZZ process_txid 1", (duration_cast<microseconds>(t1 - t0)).count());
    auto r = process_txid(txid, std::move(bct));
    auto t2 = Clock::now();
    tdiag(
        "ZZZZ process_txid 2", (duration_cast<microseconds>(t2 - t1)).count());
    return r;
}

auto BlockchainAccountActivity::process_txid(
    const Data& txid,
    std::unique_ptr<const blockchain::bitcoin::block::Transaction>
        pTX) noexcept  // TODO : MT-83
    -> std::optional<AccountActivityRowID>
{
    auto t0 = Clock::now();
    const auto rowID = AccountActivityRowID{
        blockchain_thread_item_id(Widget::api_.Crypto(), chain_, txid),
        proto::PAYMENTEVENTTYPE_COMPLETE};
    auto t1 = Clock::now();

    if (false == bool(pTX)) { return std::nullopt; }

    const auto& tx = pTX->Internal();

    if (false == contains(tx.Chains(), chain_)) { return std::nullopt; }
    auto t2 = Clock::now();

    const auto sortKey{tx.Timestamp()};
    const auto conf = [&]() -> int {
        const auto height = tx.ConfirmationHeight();

        if ((0 > height) || (height > height_)) { return 0; }

        return static_cast<int>(height_ - height) + 1;
    }();
    auto t3 = Clock::now();
    auto custom = CustomData{
        new proto::PaymentWorkflow(),
        new proto::PaymentEvent(),
        const_cast<void*>(static_cast<const void*>(pTX.release())),
        new blockchain::Type{chain_},
        new UnallocatedCString{
            Widget::api_.Crypto().Blockchain().ActivityDescription(
                primary_id_, chain_, tx)},
        new OTData{tx.ID()},
        new int{conf},
    };
    auto t4 = Clock::now();

    add_item(rowID, sortKey, custom);
    auto t5 = Clock::now();
    using namespace std::chrono;
    std::string d =
        std::to_string((duration_cast<microseconds>(t1 - t0)).count()) + " " +
        std::to_string((duration_cast<microseconds>(t2 - t1)).count()) + " " +
        std::to_string((duration_cast<microseconds>(t3 - t2)).count()) + " " +
        std::to_string((duration_cast<microseconds>(t4 - t3)).count()) + " " +
        std::to_string((duration_cast<microseconds>(t5 - t4)).count());
    tdiag("ZZZZ process_txid A [us]", d);
    return std::move(rowID);
}

auto BlockchainAccountActivity::Send(
    const UnallocatedCString& address,
    const Amount& amount,
    const UnallocatedCString& memo) const noexcept -> bool
{
    try {
        const auto& network =
            Widget::api_.Network().Blockchain().GetChain(chain_);
        const auto recipient = Widget::api_.Factory().PaymentCode(address);

        if (0 < recipient.Version()) {
            network.SendToPaymentCode(primary_id_, recipient, amount, memo);
        } else {
            network.SendToAddress(primary_id_, address, amount, memo);
        }

        return true;
    } catch (...) {

        return false;
    }
}

auto BlockchainAccountActivity::Send(
    const UnallocatedCString& address,
    const UnallocatedCString& amount,
    const UnallocatedCString& memo,
    Scale scale) const noexcept -> bool
{
    try {

        const auto& definition =
            display::GetDefinition(BlockchainToUnit(chain_));
        return Send(address, definition.Import(amount, scale), memo);
    } catch (...) {

        return false;
    }
}

auto BlockchainAccountActivity::startup() noexcept -> void
{
    tdiag("ZZZZ startup");
    load_thread();
}

auto BlockchainAccountActivity::ValidateAddress(
    const UnallocatedCString& in) const noexcept -> bool
{
    {
        const auto code = Widget::api_.Factory().PaymentCode(in);

        if (0 < code.Version()) { return true; }
    }

    using Style = blockchain::crypto::AddressStyle;

    const auto [data, style, chains, supported] =
        Widget::api_.Crypto().Blockchain().DecodeAddress(in);

    if (Style::Unknown == style) { return false; }

    if (0 == chains.count(chain_)) { return false; }

    return supported;
}

auto BlockchainAccountActivity::ValidateAmount(
    const UnallocatedCString& text) const noexcept -> UnallocatedCString
{
    try {

        const auto& definition =
            display::GetDefinition(BlockchainToUnit(chain_));
        return definition.Format(definition.Import(text));
    } catch (...) {

        return {};
    }
}

auto BlockchainAccountActivity::last_job_str() const noexcept -> std::string
{
    return std::string{print(last_job_)};
}

BlockchainAccountActivity::~BlockchainAccountActivity()
{
    wait_for_startup();
    protect_shutdown([this] { shut_down(); });
}
}  // namespace opentxs::ui::implementation
