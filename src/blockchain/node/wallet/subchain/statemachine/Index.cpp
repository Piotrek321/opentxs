// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <boost/smart_ptr/detail/operator_bool.hpp>

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/wallet/subchain/statemachine/Index.hpp"  // IWYU pragma: associated

#include <boost/smart_ptr/shared_ptr.hpp>
#include <utility>

#include "blockchain/node/wallet/subchain/SubchainStateData.hpp"
#include "blockchain/node/wallet/subchain/statemachine/ElementCache.hpp"
#include "internal/api/crypto/Blockchain.hpp"
#include "internal/blockchain/node/wallet/Types.hpp"
#include "internal/blockchain/node/wallet/subchain/statemachine/Job.hpp"
#include "internal/blockchain/node/wallet/subchain/statemachine/Types.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::blockchain::node::wallet
{
Index::Imp::Imp(
    const boost::shared_ptr<const SubchainStateData>& parent,
    const network::zeromq::BatchID batch,
    allocator_type alloc) noexcept
    : Job(LogTrace(),
          parent,
          batch,
          JobType::index,
          alloc,
          {
              {CString{
                   parent->api_.Crypto().Blockchain().Internal().KeyEndpoint(),
                   alloc},
               Direction::Connect},
          },
          {
              {parent->to_index_endpoint_, Direction::Bind},
          },
          {},
          {
              {SocketType::Push,
               {
                   {parent->to_rescan_endpoint_, Direction::Connect},
               }},
          })
    , to_rescan_(pipeline_.Internal().ExtraSocket(1))
    , last_indexed_(std::nullopt)
{
    tdiag("Imp::Imp");
}

Index::Imp::~Imp() { tdiag("Imp::~Imp"); }

auto Index::Imp::do_process_update(Message&& msg) noexcept -> void
{
    tdiag("do_process_update(&&)");

    auto clean = Set<ScanStatus>{get_allocator()};
    auto dirty = Set<block::Position>{get_allocator()};
    decode(parent_.api_, msg, clean, dirty);

    for (const auto& [type, position] : clean) {
        if (ScanState::processed == type) {
            log_(OT_PRETTY_CLASS())(parent_.name_)(" re-indexing ")(
                parent_.name_)(" due to processed block ")(position)
                .Flush();
        }
    }

    do_work();
    to_rescan_.SendDeferred(std::move(msg));
}

auto Index::Imp::do_startup() noexcept -> void
{
    last_indexed_ = parent_.db_.SubchainLastIndexed(parent_.db_key_);
    do_work();
    log_(OT_PRETTY_CLASS())(parent_.name_)(" notifying scan task to begin work")
        .Flush();
}

auto Index::Imp::done(database::Wallet::ElementMap&& elements) noexcept -> void
{
    auto& db = parent_.db_;
    const auto& index = parent_.db_key_;
    db.SubchainAddElements(index, elements);
    last_indexed_ = parent_.db_.SubchainLastIndexed(index);
    parent_.element_cache_.lock()->Add(std::move(elements));
}

auto Index::Imp::ProcessReorg(
    const Lock& headerOracleLock,
    const block::Position& parent) noexcept -> void
{
    // NOTE no action required
}

auto Index::Imp::process_do_rescan(Message&& in) noexcept -> void
{
    to_rescan_.Send(std::move(in));
}

auto Index::Imp::process_filter(Message&& in, block::Position&&) noexcept
    -> void
{
    to_rescan_.Send(std::move(in));
}

auto Index::Imp::process_key(Message&& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(4u < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();

    if (chain != parent_.chain_) { return; }

    const auto owner = parent_.api_.Factory().NymID(body.at(2));

    if (owner != parent_.owner_) { return; }

    const auto id = parent_.api_.Factory().Identifier(body.at(3));

    if (id != parent_.id_) { return; }

    const auto subchain = body.at(4).as<crypto::Subchain>();

    if (subchain != parent_.subchain_) { return; }

    do_work();
}

auto Index::Imp::work() noexcept -> int
{
    const auto need = need_index(last_indexed_);

    if (need.has_value()) { process(last_indexed_, need.value()); }

    Job::work();

    return -1;
}
}  // namespace opentxs::blockchain::node::wallet

namespace opentxs::blockchain::node::wallet
{
Index::Index(boost::shared_ptr<Imp>&& imp) noexcept
    : imp_(std::move(imp))
{
    imp_->Init(imp_);
}

Index::Index(Index&& rhs) noexcept
    : imp_(std::move(rhs.imp_))
{
}

auto Index::ChangeState(const State state, StateSequence reorg) noexcept -> bool
{
    return imp_->ChangeState(state, reorg);
}

auto Index::ProcessReorg(
    const Lock& headerOracleLock,
    const block::Position& parent) noexcept -> void
{
    imp_->ProcessReorg(headerOracleLock, parent);
}

Index::~Index()
{
    if (imp_) { imp_->Shutdown(); }
}
}  // namespace opentxs::blockchain::node::wallet
