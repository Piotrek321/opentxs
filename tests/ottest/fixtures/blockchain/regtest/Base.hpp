// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>

#include "ottest/fixtures/blockchain/TXOState.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace ottest
{
class BlockListener;
class BlockchainStartup;
class MinedBlocks;
class PeerListener;
class User;
class WalletListener;
struct TXOState;
}  // namespace ottest
// NOLINTEND(modernize-concat-nested-namespaces)

namespace ottest
{
class Regtest_fixture_base : virtual public ::testing::Test
{
public:
    static auto MaturationInterval() noexcept -> ot::blockchain::block::Height;

protected:
    using Height = ot::blockchain::block::Height;
    using Transaction = ot::api::session::Factory::Transaction_p;
    using Transactions = ot::UnallocatedDeque<ot::blockchain::block::pTxid>;
    using Generator = std::function<Transaction(Height)>;
    using Outpoint = ot::blockchain::block::Outpoint;
    using Script = ot::blockchain::bitcoin::block::Script;
    using UTXO = ot::blockchain::node::Wallet::UTXO;
    using Key = ot::ByteArray;
    using Amount = ot::Amount;
    using Pattern = ot::blockchain::bitcoin::block::Script::Pattern;
    using OutpointMetadata = std::tuple<Key, Amount, Pattern>;
    using Expected = ot::UnallocatedMap<Outpoint, OutpointMetadata>;
    using Subchain = ot::blockchain::crypto::Subchain;

    static bool init_;
    static Expected expected_;
    static Transactions transactions_;
    static ot::blockchain::block::Height height_;
    static std::optional<BlockchainStartup> miner_startup_s_;
    static std::optional<BlockchainStartup> sync_server_startup_s_;
    static std::optional<BlockchainStartup> client_1_startup_s_;
    static std::optional<BlockchainStartup> client_2_startup_s_;

    const ot::api::Context& ot_;
    const ot::Options client_args_;
    const int client_count_;
    const ot::api::session::Client& miner_;
    const ot::api::session::Client& sync_server_;
    const ot::api::session::Client& client_1_;
    const ot::api::session::Client& client_2_;
    const BlockchainStartup& miner_startup_;
    const BlockchainStartup& sync_server_startup_;
    const BlockchainStartup& client_1_startup_;
    const BlockchainStartup& client_2_startup_;
    const ot::blockchain::p2p::Address& address_;
    const PeerListener& connection_;
    const Generator default_;
    MinedBlocks& mined_blocks_;
    BlockListener& block_miner_;
    BlockListener& block_sync_server_;
    BlockListener& block_1_;
    BlockListener& block_2_;
    WalletListener& wallet_1_;
    WalletListener& wallet_2_;

    auto Account(const User& user, ot::blockchain::Type chain) noexcept
        -> const ot::blockchain::crypto::Account&;
    virtual auto Connect() noexcept -> bool;
    auto Connect(const ot::blockchain::p2p::Address& address) noexcept -> bool;
    auto Mine(const Height ancestor, const std::size_t count) noexcept -> bool;
    auto Mine(
        const Height ancestor,
        const std::size_t count,
        const Generator& gen,
        const ot::UnallocatedVector<Transaction>& extra = {}) noexcept -> bool;
    auto TestUTXOs(const Expected& expected, const ot::Vector<UTXO>& utxos)
        const noexcept -> bool;
    auto TestWallet(const ot::api::session::Client& api, const TXOState& state)
        const noexcept -> bool;

    virtual auto Shutdown() noexcept -> void;
    auto Start() noexcept -> bool;
    auto Start(const ot::api::session::Client& instance) noexcept -> bool;

    Regtest_fixture_base(
        const bool waitForHandshake,
        const int clientCount,
        ot::Options clientArgs);
    Regtest_fixture_base(
        const bool waitForHandshake,
        const int clientCount,
        ot::Options minerArgs,
        ot::Options clientArgs);

private:
    using BlockListen = ot::UnallocatedMap<int, std::unique_ptr<BlockListener>>;
    using WalletListen =
        ot::UnallocatedMap<int, std::unique_ptr<WalletListener>>;

    static const ot::UnallocatedSet<ot::blockchain::node::TxoState> states_;
    static std::unique_ptr<const ot::OTBlockchainAddress> listen_address_;
    static std::unique_ptr<const PeerListener> peer_listener_;
    static std::unique_ptr<MinedBlocks> mined_block_cache_;
    static BlockListen block_listener_;
    static WalletListen wallet_listener_;

    static auto get_bytes(const Script& script) noexcept
        -> std::optional<ot::ReadView>;
    static auto init_address(const ot::api::Session& api) noexcept
        -> const ot::blockchain::p2p::Address&;
    static auto init_block(
        const int index,
        const ot::api::Session& api,
        std::string_view name) noexcept -> BlockListener&;
    static auto init_mined() noexcept -> MinedBlocks&;
    static auto init_peer(
        const bool waitForHandshake,
        const int clientCount,
        const ot::api::session::Client& miner,
        const ot::api::session::Client& syncServer,
        const ot::api::session::Client& client1,
        const ot::api::session::Client& client2) noexcept
        -> const PeerListener&;
    static auto init_wallet(
        const int index,
        const ot::api::Session& api) noexcept -> WalletListener&;

    auto compare_outpoints(
        const ot::blockchain::node::Wallet& wallet,
        const TXOState::Data& data) const noexcept -> bool;
    auto compare_outpoints(
        const ot::blockchain::node::Wallet& wallet,
        const ot::identifier::Nym& nym,
        const TXOState::Data& data) const noexcept -> bool;
    auto compare_outpoints(
        const ot::blockchain::node::Wallet& wallet,
        const ot::identifier::Nym& nym,
        const ot::Identifier& subaccount,
        const TXOState::Data& data) const noexcept -> bool;
    auto compare_outpoints(
        const ot::blockchain::node::TxoState type,
        const TXOState::Data& expected,
        const ot::Vector<UTXO>& got) const noexcept -> bool;
};
}  // namespace ottest
