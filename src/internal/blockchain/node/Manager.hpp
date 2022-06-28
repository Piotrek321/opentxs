// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <future>

#include "internal/blockchain/node/Types.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace blockchain
{
namespace bitcoin
{
namespace block
{
class Transaction;
}  // namespace block
}  // namespace bitcoin

namespace block
{
class Hash;
}  // namespace block

namespace node
{
namespace internal
{
class FilterOracle;
class Mempool;
}  // namespace internal

struct Endpoints;
class FilterOracle;
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace zeromq
{
namespace socket
{
class Publish;
}  // namespace socket

class Message;
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::internal
{
class Manager : virtual public node::Manager
{
public:
    virtual auto BroadcastTransaction(
        const bitcoin::block::Transaction& tx,
        const bool pushtx = false) const noexcept -> bool = 0;
    virtual auto Chain() const noexcept -> Type = 0;
    virtual auto Endpoints() const noexcept -> const node::Endpoints& = 0;
    // amount represents satoshis per 1000 bytes
    virtual auto FeeRate() const noexcept -> Amount = 0;
    auto FilterOracle() const noexcept -> const node::FilterOracle& final;
    virtual auto FilterOracleInternal() const noexcept
        -> const internal::FilterOracle& = 0;
    virtual auto GetTransactions() const noexcept
        -> UnallocatedVector<block::pTxid> = 0;
    virtual auto GetTransactions(const identifier::Nym& account) const noexcept
        -> UnallocatedVector<block::pTxid> = 0;
    virtual auto IsSynchronized() const noexcept -> bool = 0;
    virtual auto IsWalletScanEnabled() const noexcept -> bool = 0;
    virtual auto JobReady(const PeerManagerJobs type) const noexcept
        -> void = 0;
    virtual auto Mempool() const noexcept -> const internal::Mempool& = 0;
    virtual auto Reorg() const noexcept
        -> const network::zeromq::socket::Publish& = 0;
    virtual auto RequestBlock(const block::Hash& block) const noexcept
        -> bool = 0;
    virtual auto RequestBlocks(
        const UnallocatedVector<ReadView>& hashes) const noexcept -> bool = 0;
    virtual auto Submit(network::zeromq::Message&& work) const noexcept
        -> void = 0;
    virtual auto Track(network::zeromq::Message&& work) const noexcept
        -> std::future<void> = 0;
    virtual auto UpdateHeight(const block::Height height) const noexcept
        -> void = 0;
    virtual auto UpdateLocalHeight(
        const block::Position position) const noexcept -> void = 0;

    virtual auto FilterOracleInternal() noexcept -> internal::FilterOracle& = 0;
    virtual auto Shutdown() noexcept -> std::shared_future<void> = 0;
    virtual auto StartWallet() noexcept -> void = 0;

    ~Manager() override = default;
};
}  // namespace opentxs::blockchain::node::internal
