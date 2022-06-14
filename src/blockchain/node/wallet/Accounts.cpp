// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <boost/smart_ptr/detail/operator_bool.hpp>

#include "0_stdafx.hpp"                         // IWYU pragma: associated
#include "1_Internal.hpp"                       // IWYU pragma: associated
#include "blockchain/node/wallet/Accounts.hpp"  // IWYU pragma: associated

#include <boost/smart_ptr/make_shared.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "internal/blockchain/database/Wallet.hpp"
#include "internal/blockchain/node/FilterOracle.hpp"
#include "internal/blockchain/node/HeaderOracle.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/blockchain/node/wallet/Account.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "internal/network/zeromq/socket/Pipeline.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/api/session/Wallet.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/ZeroMQ.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/LMDB.hpp"
#include "util/Work.hpp"

namespace opentxs::blockchain::node::wallet
{
auto print(AccountsJobs job) noexcept -> std::string_view
{
    try {
        using Job = AccountsJobs;
        static const auto map = Map<Job, CString>{
            {Job::shutdown, "shutdown"},
            {Job::nym, "nym"},
            {Job::header, "header"},
            {Job::reorg, "reorg"},
            {Job::rescan, "rescan"},
            {Job::init, "init"},
            {Job::statemachine, "statemachine"},
        };

        return map.at(job);
    } catch (...) {
        LogError()(__FUNCTION__)("invalid AccountsJobs: ")(
            static_cast<OTZMQWorkType>(job))
            .Flush();

        OT_FAIL;
    }
}
}  // namespace opentxs::blockchain::node::wallet

namespace opentxs::blockchain::node::wallet
{
Accounts::Imp::Imp(
    const api::Session& api,
    const node::internal::Manager& node,
    database::Wallet& db,
    const node::internal::Mempool& mempool,
    const network::zeromq::BatchID batch,
    const Type chain,
    CString&& shutdown,
    allocator_type alloc) noexcept
    : Actor(
          api,
          LogTrace(),
          std::string(print(chain)) + " wallet account manager",
          batch,
          alloc,
          {
              {CString{api.Endpoints().BlockchainReorg(), alloc},
               Direction::Connect},
              {CString{api.Endpoints().NymCreated(), alloc},
               Direction::Connect},
          },
          {},
          {},
          {
              {SocketType::Publish,
               {
                   {shutdown, Direction::Bind},
               }},
          })
    , api_(api)
    , node_(node)
    , db_(db)
    , mempool_(mempool)
    , chain_(chain)
    , filter_type_(node_.FilterOracleInternal().DefaultType())
    , to_children_endpoint_(std::move(shutdown))
    , to_children_(pipeline_.Internal().ExtraSocket(0))
    , state_(State::normal)
    , reorg_counter_(0)
    , accounts_(alloc)
    , startup_reorg_(std::nullopt)
{
}

Accounts::Imp::Imp(
    const api::Session& api,
    const node::internal::Manager& node,
    database::Wallet& db,
    const node::internal::Mempool& mempool,
    const network::zeromq::BatchID batch,
    const Type chain,
    allocator_type alloc) noexcept
    : Imp(api,
          node,
          db,
          mempool,
          batch,
          chain,
          network::zeromq::MakeArbitraryInproc(alloc.resource()),
          std::move(alloc))
{
}

auto Accounts::Imp::do_shutdown() noexcept -> void { accounts_.clear(); }

auto Accounts::Imp::do_startup() noexcept -> void
{
    for (const auto& id : api_.Wallet().LocalNyms()) { process_nym(id); }

    const auto oldPosition = db_.GetPosition();
    log_(OT_PRETTY_CLASS())(name_)(" last wallet position is ")(
        print(oldPosition))
        .Flush();

    if (0 > oldPosition.first) { return; }

    const auto [parent, best] = node_.HeaderOracle().CommonParent(oldPosition);

    if (parent == oldPosition) {
        log_(OT_PRETTY_CLASS())(name_)(" last wallet position is in best chain")
            .Flush();
    } else {
        log_(OT_PRETTY_CLASS())(name_)(" last wallet position is stale")
            .Flush();
        startup_reorg_.emplace(++reorg_counter_);

        auto work = MakeWork(Work::reorg);
        work.AddFrame(chain_);
        work.AddFrame(parent.second);
        work.AddFrame(parent.first);
        work.AddFrame(best.second);
        work.AddFrame(best.first);
        pipeline_.Push(std::move(work));
    }
}

auto Accounts::Imp::to_str(Work w) const noexcept -> std::string
{
    return std::string(print(w));
}

auto Accounts::Imp::pipeline(const Work work, Message&& msg) noexcept -> void
{
    tadiag("pipeline ", std::string{print(work)});

    switch (state_) {
        case State::normal: {
            state_normal(work, std::move(msg));
        } break;
        case State::shutdown: {
            shutdown_actor();
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto Accounts::Imp::process_block_header(Message&& in) noexcept -> void
{
    const auto body = in.Body();

    if (startup_reorg_.has_value()) { defer(std::move(in)); }

    if (3 >= body.size()) {
        LogError()(OT_PRETTY_CLASS())(name_)(": invalid message").Flush();

        OT_FAIL;
    }

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain_ != chain) { return; }

    const auto position =
        block::Position{body.at(3).as<block::Height>(), body.at(2).Bytes()};
    log_(OT_PRETTY_CLASS())("processing block header for ")(print(position))
        .Flush();
    db_.AdvanceTo(position);
}

auto Accounts::Imp::process_nym(const identifier::Nym& nym) noexcept -> bool
{
    const auto endpoint =
        network::zeromq::MakeArbitraryInproc(get_allocator().resource());

    if (auto i = accounts_.find(nym); accounts_.end() == i) {
        LogConsole()("Initializing ")(name_)(" wallet for ")(nym).Flush();
        const auto& account = api_.Crypto().Blockchain().Account(nym, chain_);
        account.Internal().Startup();
        auto [it, added] = accounts_.try_emplace(
            nym,
            api_,
            account,
            node_,
            db_,
            mempool_,
            chain_,
            filter_type_,
            to_children_endpoint_);

        return added;
    } else {

        return false;
    }
}

auto Accounts::Imp::process_nym(Message&& in) noexcept -> bool
{
    const auto body = in.Body();

    OT_ASSERT(1 < body.size());

    const auto id = api_.Factory().NymID(body.at(1));

    if (0 == id->size()) { return false; }

    return process_nym(id);
}

auto Accounts::Imp::process_reorg(Message&& in) noexcept -> void
{
    const auto body = in.Body();

    if (6 > body.size()) {
        LogError()(OT_PRETTY_CLASS())(name_)(": invalid message").Flush();

        OT_FAIL;
    }

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain_ != chain) { return; }

    process_reorg(
        std::move(in),
        std::make_pair(body.at(3).as<block::Height>(), body.at(2).Bytes()),
        std::make_pair(body.at(5).as<block::Height>(), body.at(4).Bytes()));
}

auto Accounts::Imp::process_reorg(
    Message&& in,
    const block::Position& ancestor,
    const block::Position& tip) noexcept -> void
{
    if (!transition_state_reorg()) {
        LogError()(OT_PRETTY_CLASS())(
            name_)(" failed to transaction to reorg state (possibly due to "
                   "startup condition")
            .Flush();
        pipeline_.Push(std::move(in));

        return;
    }

    auto errors = std::atomic_int{0};

    {
        auto lock = Lock{node_.HeaderOracle().Internal().GetMutex()};
        auto tx = db_.StartReorg();
        for_each([&](auto& a) {
            a.second.ProcessReorg(lock, tx, errors, ancestor);
        });

        if (!db_.FinalizeReorg(tx, ancestor)) { ++errors; }

        try {
            if (0 < errors) { throw std::runtime_error{"Prepare step failed"}; }

            if (!tx.Finalize(true)) {

                throw std::runtime_error{"Finalize transaction failed"};
            }
        } catch (const std::exception& e) {
            LogError()(OT_PRETTY_CLASS())(name_)(": ")(e.what()).Flush();

            OT_FAIL;
        }
    }

    try {
        if (!db_.AdvanceTo(tip)) {

            throw std::runtime_error{"Advance chain failed"};
        }
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(name_)(": ")(e.what()).Flush();

        OT_FAIL;
    }

    // TODO ensure filter oracle has processed the reorg before proceeding
    for_each([&](auto& a) {
        const auto rc = a.second.ChangeState(Account::State::normal);

        OT_ASSERT(rc);
    });
    LogConsole()(name_)(": reorg to ")(print(tip))(" finished").Flush();
}

auto Accounts::Imp::process_rescan(Message&& in) noexcept -> void
{
    to_children_.Send(MakeWork(AccountJobs::rescan));
}

auto Accounts::Imp::Rescan() const noexcept -> void
{
    pipeline_.Push(MakeWork(Work::rescan));
}

auto Accounts::Imp::Shutdown() noexcept -> void
{
    // WARNING this function must never be called from with this class's
    // Actor::worker function or else a deadlock will occur. Shutdown must only
    // be called by a different Actor.
    transition_state_shutdown();
}

auto Accounts::Imp::state_normal(const Work work, Message&& msg) noexcept
    -> void
{
    switch (work) {
        case Work::shutdown: {
            shutdown_actor();
        } break;
        case Work::nym: {
            process_nym(std::move(msg));
        } break;
        case Work::header: {
            process_block_header(std::move(msg));
        } break;
        case Work::reorg: {
            process_reorg(std::move(msg));
        } break;
        case Work::rescan: {
            process_rescan(std::move(msg));
        } break;
        case Work::init: {
            do_init();
        } break;
        case Work::statemachine: {
            do_work();
        } break;
        default: {
            LogError()(OT_PRETTY_CLASS())(name_)(": unhandled message type ")(
                static_cast<OTZMQWorkType>(work))
                .Flush();

            OT_FAIL;
        }
    }
}

auto Accounts::Imp::transition_state_reorg() noexcept -> bool
{
    const auto id =
        startup_reorg_.has_value() ? startup_reorg_.value() : ++reorg_counter_;

    log_(OT_PRETTY_CLASS())(name_)(": processing reorg ")(id).Flush();

    if (!startup_reorg_.has_value()) {
        auto work = MakeWork(AccountJobs::prepare_reorg);
        work.AddFrame(id);
        to_children_.SendDeferred(std::move(work));
        tdiag("Sending prepare_reorg");
    }

    auto success{true};

    for_each([&](auto& a) {
        success &= a.second.ChangeState(Account::State::reorg, id);
    });

    if (success) { startup_reorg_.reset(); }

    return success;
}

auto Accounts::Imp::transition_state_shutdown() noexcept -> void
{
    tdiag("About to send Shutdown");
    to_children_.SendDeferred(MakeWork(AccountJobs::prepare_shutdown));
    for_each([](auto& a) {
        auto rc = a.second.ChangeState(Account::State::shutdown);

        OT_ASSERT(rc);
    });
    state_ = State::shutdown;
    log_(OT_PRETTY_CLASS())(name_)(": transitioned to shutdown state").Flush();
    signal_shutdown();
}

auto Accounts::Imp::work() noexcept -> int { return -1; }

Accounts::Imp::~Imp() = default;
}  // namespace opentxs::blockchain::node::wallet

namespace opentxs::blockchain::node::wallet
{
Accounts::Accounts(
    const api::Session& api,
    const node::internal::Manager& node,
    database::Wallet& db,
    const node::internal::Mempool& mempool,
    const Type chain) noexcept
    : imp_([&] {
        const auto& asio = api.Network().ZeroMQ().Internal();
        const auto batchID = asio.PreallocateBatch();
        // TODO the version of libc++ present in android ndk 23.0.7599858
        // has a broken std::allocate_shared function so we're using
        // boost::shared_ptr instead of std::shared_ptr

        return boost::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)},
            api,
            node,
            db,
            mempool,
            batchID,
            chain);
    }())
{
    OT_ASSERT(imp_);
}

auto Accounts::Init() noexcept -> void { imp_->Init(imp_); }

auto Accounts::Rescan() const noexcept -> void { imp_->Rescan(); }

auto Accounts::Shutdown() noexcept -> void { imp_->Shutdown(); }

Accounts::~Accounts() { imp_->Shutdown(); }
}  // namespace opentxs::blockchain::node::wallet
