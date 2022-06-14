// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <boost/smart_ptr/detail/operator_bool.hpp>

#include "0_stdafx.hpp"                        // IWYU pragma: associated
#include "1_Internal.hpp"                      // IWYU pragma: associated
#include "blockchain/node/wallet/Account.hpp"  // IWYU pragma: associated

#include <boost/smart_ptr/make_shared.hpp>
#include <chrono>
#include <string_view>
#include <utility>

#include "blockchain/node/wallet/subchain/NotificationStateData.hpp"
#include "blockchain/node/wallet/subchain/SubchainStateData.hpp"
#include "internal/api/crypto/Blockchain.hpp"
#include "internal/api/session/Wallet.hpp"
#include "internal/blockchain/node/FilterOracle.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/blockchain/node/wallet/subchain/Subchain.hpp"
#include "internal/blockchain/node/wallet/subchain/statemachine/Types.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/crypto/Account.hpp"
#include "opentxs/blockchain/crypto/Deterministic.hpp"
#include "opentxs/blockchain/crypto/HD.hpp"
#include "opentxs/blockchain/crypto/Notification.hpp"
#include "opentxs/blockchain/crypto/PaymentCode.hpp"
#include "opentxs/blockchain/crypto/SubaccountType.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/WorkType.hpp"

namespace opentxs::blockchain::node::wallet
{
auto print(AccountJobs job) noexcept -> std::string_view
{
    try {
        using Job = AccountJobs;
        static const auto map = Map<Job, CString>{
            {Job::shutdown, "shutdown"},
            {Job::subaccount, "subaccount"},
            {Job::prepare_reorg, "prepare_reorg"},
            {Job::rescan, "rescan"},
            {Job::init, "init"},
            {Job::key, "key"},
            {Job::prepare_shutdown, "prepare_shutdown"},
            {Job::statemachine, "statemachine"},
        };

        return map.at(job);
    } catch (...) {
        LogError()(__FUNCTION__)("invalid AccountJobs: ")(
            static_cast<OTZMQWorkType>(job))
            .Flush();

        OT_FAIL;
    }
}
}  // namespace opentxs::blockchain::node::wallet

namespace opentxs::blockchain::node::wallet
{
Account::Imp::Imp(
    const api::Session& api,
    const crypto::Account& account,
    const node::internal::Manager& node,
    database::Wallet& db,
    const node::internal::Mempool& mempool,
    const network::zeromq::BatchID batch,
    const Type chain,
    const cfilter::Type filter,
    CString&& fromParent,
    allocator_type alloc) noexcept
    : Actor(
          api,
          LogTrace(),
          std::string(print(chain)) + " account for " + account.NymID().str(),
          batch,
          alloc,
          {
              {fromParent, Direction::Connect},
              {CString{
                   api.Crypto().Blockchain().Internal().KeyEndpoint(),
                   alloc},
               Direction::Connect},
              {CString{api.Endpoints().BlockchainAccountCreated(), alloc},
               Direction::Connect},
          })
    , api_(api)
    , account_(account)
    , node_(node)
    , db_(db)
    , mempool_(mempool)
    , chain_(chain)
    , filter_type_(node_.FilterOracleInternal().DefaultType())
    , from_parent_(std::move(fromParent))
    , pending_state_(State::normal)
    , state_(State::normal)
    , reorgs_(alloc)
    , notification_(alloc)
    , internal_(alloc)
    , external_(alloc)
    , outgoing_(alloc)
    , incoming_(alloc)
{
}

Account::Imp::Imp(
    const api::Session& api,
    const crypto::Account& account,
    const node::internal::Manager& node,
    database::Wallet& db,
    const node::internal::Mempool& mempool,
    const network::zeromq::BatchID batch,
    const Type chain,
    const cfilter::Type filter,
    const std::string_view fromParent,
    allocator_type alloc) noexcept
    : Imp(api,
          account,
          node,
          db,
          mempool,
          batch,
          chain,
          filter,
          CString{fromParent, alloc},
          alloc)
{
}

Account::Imp::~Imp()
{
    tdiag("Account::Imp::~Imp");
    signal_shutdown();
}

auto Account::Imp::ChangeState(const State state, StateSequence reorg) noexcept
    -> bool
{
    return synchronize(
        [state, reorg, this] { return sChangeState(state, reorg); });
}

auto Account::Imp::sChangeState(const State state, StateSequence reorg) noexcept
    -> bool
{
    if (auto old = pending_state_.exchange(state); old == state) {
        return true;
    }

    auto output{false};

    switch (state) {
        case State::normal: {
            if (State::reorg != state_) { break; }

            output = transition_state_normal();
        } break;
        case State::reorg: {
            if (State::shutdown == state_) { break; }

            output = transition_state_reorg(reorg);
        } break;
        case State::shutdown: {
            if (State::reorg == state_) { break; }

            output = transition_state_shutdown();
        } break;
        default: {
            OT_FAIL;
        }
    }

    if (!output) {
        LogError()(OT_PRETTY_CLASS())(name_)(" failed to change state from ")(
            print(state_))(" to ")(print(state))
            .Flush();
    }

    return output;
}

auto Account::Imp::check_hd(const Identifier& id) noexcept -> void
{
    check_hd(account_.GetHD().at(id));
}

auto Account::Imp::check_hd(const crypto::HD& subaccount) noexcept -> void
{
    get(subaccount, crypto::Subchain::Internal, internal_);
    get(subaccount, crypto::Subchain::External, external_);
}

auto Account::Imp::check_notification(const Identifier& id) noexcept -> void
{
    check_notification(account_.GetNotification().at(id));
}

auto Account::Imp::check_notification(
    const crypto::Notification& subaccount) noexcept -> void
{
    auto& map = notification_;

    if (0u < map.count(subaccount.ID())) { return; }

    const auto& code = subaccount.LocalPaymentCode();
    log_("Initializing payment code ")(code.asBase58())(" on ")(name_).Flush();
    const auto& asio = api_.Network().ZeroMQ().Internal();
    const auto batchID = asio.PreallocateBatch();
    auto [it, added] = map.try_emplace(
        subaccount.ID(),
        boost::allocate_shared<NotificationStateData>(
            alloc::PMR<NotificationStateData>{asio.Alloc(batchID)},
            api_,
            node_,
            db_,
            mempool_,
            filter_type_,
            crypto::Subchain::NotificationV3,
            batchID,
            from_parent_,
            code,
            subaccount));
    auto& ptr = it->second;

    OT_ASSERT(ptr);

    auto& subchain = *ptr;

    if (added) { subchain.Init(ptr); }
}

auto Account::Imp::check_pc(const Identifier& id) noexcept -> void
{
    check_pc(account_.GetPaymentCode().at(id));
}

auto Account::Imp::check_pc(const crypto::PaymentCode& subaccount) noexcept
    -> void
{
    get(subaccount, crypto::Subchain::Outgoing, outgoing_);
    get(subaccount, crypto::Subchain::Incoming, incoming_);
}

auto Account::Imp::clear_children() noexcept -> void
{
    for (auto [key, sp] : notification_) {
        sp->ChangeState(Subchain::State::shutdown, {});
        std::chrono::milliseconds x(100);
        std::this_thread::sleep_for(x);
    }
    //    std::for_each(notification_.begin(), notification_.end(), cb);
    for (auto [key, sp] : internal_) {
        sp->ChangeState(Subchain::State::shutdown, {});
    }
    //    std::for_each(notification_.begin(), internal_.end(), cb);
    for (auto [key, sp] : external_) {
        sp->ChangeState(Subchain::State::shutdown, {});
    }
    //    std::for_each(external_.begin(), external_.end(), cb);
    for (auto [key, sp] : outgoing_) {
        sp->ChangeState(Subchain::State::shutdown, {});
    }
    //    std::for_each(outgoing_.begin(), outgoing_.end(), cb);
    for (auto [key, sp] : incoming_) {
        sp->ChangeState(Subchain::State::shutdown, {});
    }
    //    std::for_each(incoming_.begin(), incoming_.end(), cb);

    //    for_each(cb);
    notification_.clear();
    internal_.clear();
    external_.clear();
    outgoing_.clear();
    incoming_.clear();
}

auto Account::Imp::do_shutdown() noexcept -> void { clear_children(); }

auto Account::Imp::do_startup() noexcept -> void
{
    api_.Wallet().Internal().PublishNym(account_.NymID());
    scan_subchains();
    index_nym(account_.NymID());
}

auto Account::Imp::get(
    const crypto::Deterministic& subaccount,
    const crypto::Subchain subchain,
    Subchains& map) noexcept -> Subchain&
{
    auto it = map.find(subaccount.ID());

    if (map.end() != it) { return *(it->second); }

    return instantiate(subaccount, subchain, map);
}

auto Account::Imp::index_nym(const identifier::Nym& id) noexcept -> void
{
    for (const auto& subaccount : account_.GetNotification()) {
        check_notification(subaccount);
    }
}

auto Account::Imp::Init(boost::shared_ptr<Imp> me) noexcept -> void
{
    signal_startup(me);
}

auto Account::Imp::instantiate(
    const crypto::Deterministic& subaccount,
    const crypto::Subchain subchain,
    Subchains& map) noexcept -> Subchain&
{
    log_("Instantiating ")(name_)(" subaccount ")(subaccount.ID())(" ")(
        print(subchain))(" subchain for ")(subaccount.Parent().NymID())
        .Flush();
    const auto& asio = api_.Network().ZeroMQ().Internal();
    const auto batchID = asio.PreallocateBatch();
    auto [it, added] = map.try_emplace(
        subaccount.ID(),
        boost::allocate_shared<DeterministicStateData>(
            alloc::PMR<DeterministicStateData>{asio.Alloc(batchID)},
            api_,
            node_,
            db_,
            mempool_,
            subaccount,
            filter_type_,
            subchain,
            batchID,
            from_parent_));
    auto& ptr = it->second;

    OT_ASSERT(ptr);

    auto& output = *ptr;

    if (added) { output.Init(ptr); }

    return output;
}

auto Account::Imp::to_str(Work w) const noexcept -> std::string
{
    return std::string(print(w));
}

auto Account::Imp::pipeline(const Work work, Message&& msg) noexcept -> void
{
    tadiag("pipeline ", std::string(print(work)));

    switch (state_) {
        case State::normal: {
            state_normal(work, std::move(msg));
        } break;
        case State::reorg: {
            state_reorg(work, std::move(msg));
        } break;
        case State::shutdown: {
            shutdown_actor();
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto Account::Imp::process_key(Message&& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(5u < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain != chain_) { return; }

    const auto owner = api_.Factory().NymID(body.at(2));

    if (owner != account_.NymID()) { return; }

    const auto id = api_.Factory().Identifier(body.at(3));
    const auto type = body.at(5).as<crypto::SubaccountType>();
    process_subaccount(id, type);
}

auto Account::Imp::process_prepare_reorg(Message&& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(1u < body.size());

    transition_state_reorg(body.at(1).as<StateSequence>());
}

auto Account::Imp::process_rescan(Message&& in) noexcept -> void
{
    // NOTE no action necessary
}

auto Account::Imp::process_subaccount(Message&& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(4 < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain != chain_) { return; }

    const auto owner = api_.Factory().NymID(body.at(2));

    if (owner != account_.NymID()) { return; }

    const auto type = body.at(3).as<crypto::SubaccountType>();
    const auto id = api_.Factory().Identifier(body.at(4));
    process_subaccount(id, type);
}

auto Account::Imp::process_subaccount(
    const Identifier& id,
    const crypto::SubaccountType type) noexcept -> void
{
    switch (type) {
        case crypto::SubaccountType::HD: {
            check_hd(id);
        } break;
        case crypto::SubaccountType::PaymentCode: {
            check_pc(id);
        } break;
        default: {

            OT_FAIL;
        }
    }
}

auto Account::Imp::ProcessReorg(
    const Lock& headerOracleLock,
    storage::lmdb::LMDB::Transaction& tx,
    std::atomic_int& errors,
    const block::Position& parent) noexcept -> void
{
    synchronize(
        [&lock = std::as_const(headerOracleLock), &tx, &errors, &parent, this] {
            sProcessReorg(lock, tx, errors, parent);
        });
}

auto Account::Imp::sProcessReorg(
    const Lock& headerOracleLock,
    storage::lmdb::LMDB::Transaction& tx,
    std::atomic_int& errors,
    const block::Position& parent) noexcept -> void
{
    for_each([&](auto& item) {
        item.second->ProcessReorg(headerOracleLock, tx, errors, parent);
    });
}

auto Account::Imp::scan_subchains() noexcept -> void
{
    for (const auto& subaccount : account_.GetHD()) { check_hd(subaccount); }

    for (const auto& subaccount : account_.GetPaymentCode()) {
        check_pc(subaccount);
    }
}

auto Account::Imp::state_normal(const Work work, Message&& msg) noexcept -> void
{
    switch (work) {
        case Work::shutdown: {
            shutdown_actor();
        } break;
        case Work::subaccount: {
            process_subaccount(std::move(msg));
        } break;
        case Work::prepare_reorg: {
            process_prepare_reorg(std::move(msg));
        } break;
        case Work::rescan: {
            process_rescan(std::move(msg));
        } break;
        case Work::init: {
            do_init();
        } break;
        case Work::key: {
            process_key(std::move(msg));
        } break;
        case Work::prepare_shutdown: {
            transition_state_shutdown();
        } break;
        case Work::statemachine: {
            do_work();
        } break;
        default: {
            LogError()(OT_PRETTY_CLASS())(name_)(" unhandled message type ")(
                static_cast<OTZMQWorkType>(work))
                .Flush();

            OT_FAIL;
        }
    }
}

auto Account::Imp::state_reorg(const Work work, Message&& msg) noexcept -> void
{
    switch (work) {
        case Work::subaccount:
        case Work::prepare_reorg:
        case Work::rescan:
        case Work::key:
        case Work::statemachine: {
            defer(std::move(msg));
        } break;
        case Work::shutdown:
        case Work::prepare_shutdown:
        case Work::init: {
            LogError()(OT_PRETTY_CLASS())(name_)(" wrong state for ")(
                print(work))(" message")
                .Flush();

            OT_FAIL;
        }
        default: {
            LogError()(OT_PRETTY_CLASS())(name_)(" unhandled message type ")(
                static_cast<OTZMQWorkType>(work))
                .Flush();

            OT_FAIL;
        }
    }
}

auto Account::Imp::transition_state_normal() noexcept -> bool
{
    disable_automatic_processing(false);
    const auto cb = [](auto& value) {
        auto rc = value.second->ChangeState(Subchain::State::normal, {});

        OT_ASSERT(rc);
    };
    for_each(cb);
    state_ = State::normal;
    log_(OT_PRETTY_CLASS())(name_)(" transitioned to normal state ").Flush();
    trigger();

    return true;
}

auto Account::Imp::transition_state_reorg(StateSequence id) noexcept -> bool
{
    OT_ASSERT(0u < id);

    auto success{true};

    if (0u == reorgs_.count(id)) {
        const auto cb = [&](auto& value) {
            success &= value.second->ChangeState(Subchain::State::reorg, id);
        };
        for_each(cb);

        if (!success) { return false; }

        reorgs_.emplace(id);
        disable_automatic_processing(true);
        state_ = State::reorg;
        log_(OT_PRETTY_CLASS())(name_)(" ready to process reorg ")(id).Flush();
    } else {
        log_(OT_PRETTY_CLASS())(name_)(" reorg ")(id)(" already handled")
            .Flush();
    }

    return true;
}

auto Account::Imp::transition_state_shutdown() noexcept -> bool
{
    clear_children();
    state_ = State::shutdown;
    log_(OT_PRETTY_CLASS())(name_)(" transitioned to shutdown state ").Flush();
    signal_shutdown();

    return true;
}

auto Account::Imp::work() noexcept -> int { return -1; }
}  // namespace opentxs::blockchain::node::wallet

namespace opentxs::blockchain::node::wallet
{
Account::Account(
    const api::Session& api,
    const crypto::Account& account,
    const node::internal::Manager& node,
    database::Wallet& db,
    const node::internal::Mempool& mempool,
    const Type chain,
    const cfilter::Type filter,
    const std::string_view shutdown) noexcept
    : imp_([&] {
        const auto& asio = api.Network().ZeroMQ().Internal();
        const auto batchID = asio.PreallocateBatch();
        // TODO the version of libc++ present in android ndk 23.0.7599858
        // has a broken std::allocate_shared function so we're using
        // boost::shared_ptr instead of std::shared_ptr

        return boost::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)},
            api,
            account,
            node,
            db,
            mempool,
            batchID,
            chain,
            filter,
            shutdown);
    }())
{
    OT_ASSERT(imp_);

    imp_->Init(imp_);
}

Account::Account(Account&& rhs) noexcept
    : imp_(std::move(rhs.imp_))
{
    OT_ASSERT(imp_);
}

auto Account::ProcessReorg(
    const Lock& headerOracleLock,
    storage::lmdb::LMDB::Transaction& tx,
    std::atomic_int& errors,
    const block::Position& parent) noexcept -> void
{
    imp_->ProcessReorg(headerOracleLock, tx, errors, parent);
}

auto Account::ChangeState(const State state, StateSequence reorg) noexcept
    -> bool
{
    return imp_->ChangeState(state, reorg);
}

Account::~Account()
{
    if (imp_) { imp_->Shutdown(); }
}
}  // namespace opentxs::blockchain::node::wallet
