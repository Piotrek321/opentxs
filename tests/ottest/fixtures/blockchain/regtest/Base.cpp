// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/regtest/Base.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <type_traits>
#include <utility>

#include "internal/blockchain/Params.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/P0330.hpp"
#include "ottest/fixtures/blockchain/BlockListener.hpp"
#include "ottest/fixtures/blockchain/BlockchainStartup.hpp"
#include "ottest/fixtures/blockchain/Common.hpp"
#include "ottest/fixtures/blockchain/MinedBlocks.hpp"
#include "ottest/fixtures/blockchain/PeerListener.hpp"
#include "ottest/fixtures/blockchain/WalletListener.hpp"
#include "ottest/fixtures/common/User.hpp"

namespace ottest
{
bool Regtest_fixture_base::init_{false};
Regtest_fixture_base::Expected Regtest_fixture_base::expected_{};
Regtest_fixture_base::Transactions Regtest_fixture_base::transactions_{};
ot::blockchain::block::Height Regtest_fixture_base::height_{0};
std::optional<BlockchainStartup> Regtest_fixture_base::miner_startup_s_{};
std::optional<BlockchainStartup> Regtest_fixture_base::sync_server_startup_s_{};
std::optional<BlockchainStartup> Regtest_fixture_base::client_1_startup_s_{};
std::optional<BlockchainStartup> Regtest_fixture_base::client_2_startup_s_{};
using TxoState = ot::blockchain::node::TxoState;
const ot::UnallocatedSet<TxoState> Regtest_fixture_base::states_{
    TxoState::UnconfirmedNew,
    TxoState::UnconfirmedSpend,
    TxoState::ConfirmedNew,
    TxoState::ConfirmedSpend,
    TxoState::All,
};
std::unique_ptr<const ot::OTBlockchainAddress>
    Regtest_fixture_base::listen_address_{};
std::unique_ptr<const PeerListener> Regtest_fixture_base::peer_listener_{};
std::unique_ptr<MinedBlocks> Regtest_fixture_base::mined_block_cache_{};
Regtest_fixture_base::BlockListen Regtest_fixture_base::block_listener_{};
Regtest_fixture_base::WalletListen Regtest_fixture_base::wallet_listener_{};
}  // namespace ottest

namespace ottest
{
using namespace opentxs::literals;

Regtest_fixture_base::Regtest_fixture_base(
    const ot::api::Context& ot,
    const bool waitForHandshake,
    const int clientCount,
    ot::Options minerArgs,
    ot::Options clientArgs)
    : client_args_(std::move(
          clientArgs.SetBlockchainProfile(opentxs::BlockchainProfile::desktop)
              .AddBlockchainSyncServer(sync_server_main_endpoint_)))
    , client_count_(clientCount)
    , miner_(ot.StartClientSession(
          minerArgs.SetBlockchainProfile(ot::BlockchainProfile::server)
              .SetBlockchainWalletEnabled(false),
          0))
    , sync_server_(ot.StartClientSession(
          ot::Options{}
              .SetBlockchainProfile(ot::BlockchainProfile::server)
              .SetBlockchainWalletEnabled(false)
              .SetBlockchainSyncEnabled(true),
          1))
    , client_1_(ot.StartClientSession(client_args_, 2))
    , client_2_(ot.StartClientSession(client_args_, 3))
    , miner_startup_([&]() -> auto& {
        if (false == miner_startup_s_.has_value()) {
            miner_startup_s_.emplace(miner_, test_chain_);
        }

        return miner_startup_s_.value();
    }())
    , sync_server_startup_([&]() -> auto& {
        if (false == sync_server_startup_s_.has_value()) {
            sync_server_startup_s_.emplace(sync_server_, test_chain_);
        }

        return sync_server_startup_s_.value();
    }())
    , client_1_startup_([&]() -> auto& {
        if (false == client_1_startup_s_.has_value()) {
            client_1_startup_s_.emplace(client_1_, test_chain_);
        }

        return client_1_startup_s_.value();
    }())
    , client_2_startup_([&]() -> auto& {
        if (false == client_2_startup_s_.has_value()) {
            client_2_startup_s_.emplace(client_2_, test_chain_);
        }

        return client_2_startup_s_.value();
    }())
    , address_(init_address(miner_))
    , connection_(init_peer(
          waitForHandshake,
          client_count_,
          miner_,
          sync_server_,
          client_1_,
          client_2_))
    , default_([&](Height height) -> Transaction {
        using OutputBuilder = ot::api::session::Factory::OutputBuilder;

        return miner_.Factory().BitcoinGenerationTransaction(
            test_chain_,
            height,
            [&] {
                auto output = ot::UnallocatedVector<OutputBuilder>{};
                const auto text = ot::UnallocatedCString{"null"};
                const auto keys =
                    ot::UnallocatedSet<ot::blockchain::crypto::Key>{};
                output.emplace_back(
                    5000000000,
                    miner_.Factory().BitcoinScriptNullData(test_chain_, {text}),
                    keys);

                return output;
            }(),
            coinbase_fun_);
    })
    , mined_blocks_(init_mined())
    , block_miner_(init_block(0, miner_, "miner"))
    , block_sync_server_(init_block(1, sync_server_, "sync server"))
    , block_1_(init_block(2, client_1_, "client 1"))
    , block_2_(init_block(3, client_2_, "client 2"))
    , wallet_1_(init_wallet(0, client_1_))
    , wallet_2_(init_wallet(1, client_2_))
{
}

Regtest_fixture_base::Regtest_fixture_base(
    const ot::api::Context& ot,
    const bool waitForHandshake,
    const int clientCount,
    ot::Options clientArgs)
    : Regtest_fixture_base(
          ot,
          waitForHandshake,
          clientCount,
          ot::Options{},
          std::move(clientArgs))
{
}

auto Regtest_fixture_base::Account(
    const User& user,
    ot::blockchain::Type chain) noexcept
    -> const ot::blockchain::crypto::Account&
{
    return user.api_->Crypto().Blockchain().Account(user.nym_id_, chain);
}

auto Regtest_fixture_base::compare_outpoints(
    const ot::blockchain::node::Wallet& wallet,
    const TXOState::Data& data) const noexcept -> bool
{
    auto output{true};

    for (const auto state : states_) {
        output &= compare_outpoints(state, data, wallet.GetOutputs(state));
    }

    return output;
}

auto Regtest_fixture_base::compare_outpoints(
    const ot::blockchain::node::Wallet& wallet,
    const ot::identifier::Nym& nym,
    const TXOState::Data& data) const noexcept -> bool
{
    auto output{true};

    for (const auto state : states_) {
        output &= compare_outpoints(state, data, wallet.GetOutputs(nym, state));
    }

    return output;
}

auto Regtest_fixture_base::compare_outpoints(
    const ot::blockchain::node::Wallet& wallet,
    const ot::identifier::Nym& nym,
    const ot::identifier::Generic& subaccount,
    const TXOState::Data& data) const noexcept -> bool
{
    auto output{true};

    for (const auto state : states_) {
        output &= compare_outpoints(
            state, data, wallet.GetOutputs(nym, subaccount, state));
    }

    return output;
}

auto Regtest_fixture_base::compare_outpoints(
    const ot::blockchain::node::TxoState type,
    const TXOState::Data& expected,
    const ot::Vector<UTXO>& got) const noexcept -> bool
{
    auto output{true};
    static const auto emptySet =
        ot::UnallocatedSet<ot::blockchain::block::Outpoint>{};
    const auto& set = [&]() -> auto&
    {
        try {

            return expected.data_.at(type);
        } catch (...) {

            return emptySet;
        }
    }
    ();
    output &= set.size() == got.size();

    EXPECT_EQ(set.size(), got.size());

    for (const auto& [outpoint, pOutput] : got) {
        const auto have = (1 == set.count(outpoint));
        const auto match = TestUTXOs(expected_, got);

        output &= have;
        output &= match;

        EXPECT_TRUE(have);
        EXPECT_TRUE(match);
    }

    return output;
}

auto Regtest_fixture_base::Connect() noexcept -> bool
{
    return Connect(address_);
}

auto Regtest_fixture_base::Connect(
    const ot::blockchain::p2p::Address& address) noexcept -> bool
{
    const auto miner = [&]() -> std::function<bool()> {
        const auto handle = miner_.Network().Blockchain().GetChain(test_chain_);

        EXPECT_TRUE(handle);

        if (false == handle.IsValid()) {
            return [] { return false; };
        }

        const auto& miner = handle.get();
        const auto listen = miner.Listen(address);
        const auto target = client_count_ + 1;

        EXPECT_TRUE(listen);

        return [=] {
            EXPECT_EQ(connection_.miner_1_peers_, target);

            return listen && (target == connection_.miner_1_peers_);
        };
    }();
    const auto syncServer = [&]() -> std::function<bool()> {
        const auto handle =
            sync_server_.Network().Blockchain().GetChain(test_chain_);

        EXPECT_TRUE(handle);

        if (false == handle.IsValid()) {
            return [] { return false; };
        }

        const auto& client = handle.get();
        const auto added = client.AddPeer(address);
        const auto started =
            sync_server_.Network().Blockchain().StartSyncServer(
                sync_server_main_endpoint_,
                sync_server_main_endpoint_,
                sync_server_push_endpoint_,
                sync_server_push_endpoint_);

        EXPECT_TRUE(added);
        EXPECT_TRUE(started);

        return [=] {
            EXPECT_GT(connection_.sync_server_peers_, 0);

            return added && started && (0 < connection_.sync_server_peers_);
        };
    }();
    const auto client1 = [&]() -> std::function<bool()> {
        if (0 < client_count_) {
            const auto handle =
                client_1_.Network().Blockchain().GetChain(test_chain_);

            EXPECT_TRUE(handle);

            if (false == handle.IsValid()) {
                return [] { return false; };
            }

            const auto& client = handle.get();
            const auto added = client.AddPeer(address);

            EXPECT_TRUE(added);

            return [=] {
                EXPECT_GT(connection_.client_1_peers_, 0);

                return added && (0 < connection_.client_1_peers_);
            };
        } else {

            return [] { return true; };
        }
    }();
    const auto client2 = [&]() -> std::function<bool()> {
        if (1 < client_count_) {
            const auto handle =
                client_2_.Network().Blockchain().GetChain(test_chain_);

            EXPECT_TRUE(handle);

            if (false == handle.IsValid()) {
                return [] { return false; };
            }

            const auto& client = handle.get();
            const auto added = client.AddPeer(address);

            EXPECT_TRUE(added);

            return [=] {
                EXPECT_GT(connection_.client_2_peers_, 0);

                return added && (0 < connection_.client_2_peers_);
            };
        } else {

            return [] { return true; };
        }
    }();

    const auto status = connection_.done_.wait_for(2min);
    const auto future = (std::future_status::ready == status);

    OT_ASSERT(future);

    return future && miner() && syncServer() && client1() && client2();
}

auto Regtest_fixture_base::get_bytes(const Script& script) noexcept
    -> std::optional<ot::ReadView>
{
    switch (script.Type()) {
        case Pattern::PayToPubkey: {

            return script.Pubkey();
        }
        case Pattern::PayToPubkeyHash: {

            return script.PubkeyHash();
        }
        case Pattern::PayToMultisig: {

            return script.MultisigPubkey(0);
        }
        default: {

            return std::nullopt;
        }
    }
}

auto Regtest_fixture_base::init_address(const ot::api::Session& api) noexcept
    -> const ot::blockchain::p2p::Address&
{
    constexpr auto test_endpoint{"inproc://test_endpoint"};
    constexpr auto test_port = std::uint16_t{18444};

    if (false == bool(listen_address_)) {
        listen_address_ = std::make_unique<ot::OTBlockchainAddress>(
            api.Factory().BlockchainAddress(
                ot::blockchain::p2p::Protocol::bitcoin,
                ot::blockchain::p2p::Network::zmq,
                api.Factory().DataFromBytes(
                    ot::UnallocatedCString{test_endpoint}),
                test_port,
                test_chain_,
                {},
                {}));
    }

    OT_ASSERT(listen_address_);

    return *listen_address_;
}

auto Regtest_fixture_base::init_block(
    const int index,
    const ot::api::Session& api,
    std::string_view name) noexcept -> BlockListener&
{
    auto& p = block_listener_[index];

    if (false == bool(p)) { p = std::make_unique<BlockListener>(api, name); }

    OT_ASSERT(p);

    return *p;
}

auto Regtest_fixture_base::init_mined() noexcept -> MinedBlocks&
{
    if (false == bool(mined_block_cache_)) {
        mined_block_cache_ = std::make_unique<MinedBlocks>();
    }

    OT_ASSERT(mined_block_cache_);

    return *mined_block_cache_;
}

auto Regtest_fixture_base::init_peer(
    const bool waitForHandshake,
    const int clientCount,
    const ot::api::session::Client& miner,
    const ot::api::session::Client& syncServer,
    const ot::api::session::Client& client1,
    const ot::api::session::Client& client2) noexcept -> const PeerListener&
{
    if (false == bool(peer_listener_)) {
        peer_listener_ = std::make_unique<PeerListener>(
            waitForHandshake, clientCount, miner, syncServer, client1, client2);
    }

    OT_ASSERT(peer_listener_);

    return *peer_listener_;
}

auto Regtest_fixture_base::init_wallet(
    const int index,
    const ot::api::Session& api) noexcept -> WalletListener&
{
    auto& p = wallet_listener_[index];

    if (false == bool(p)) { p = std::make_unique<WalletListener>(api); }

    OT_ASSERT(p);

    return *p;
}

auto Regtest_fixture_base::MaturationInterval() noexcept
    -> ot::blockchain::block::Height
{
    static const auto interval =
        ot::blockchain::params::Chains().at(test_chain_).maturation_interval_;

    return interval;
}

auto Regtest_fixture_base::Mine(
    const Height ancestor,
    const std::size_t count) noexcept -> bool
{
    return Mine(ancestor, count, default_);
}

auto Regtest_fixture_base::Mine(
    const Height ancestor,
    const std::size_t count,
    const Generator& gen,
    const ot::UnallocatedVector<Transaction>& extra) noexcept -> bool
{
    const auto targetHeight = ancestor + static_cast<Height>(count);
    auto blocks = ot::UnallocatedVector<BlockListener::Future>{};
    auto wallets = ot::UnallocatedVector<WalletListener::Future>{};
    blocks.reserve(client_count_ + 1_uz);
    wallets.reserve(client_count_);
    blocks.emplace_back(block_miner_.GetFuture(targetHeight));

    if (0 < client_count_) {
        blocks.emplace_back(block_1_.GetFuture(targetHeight));
        wallets.emplace_back(wallet_1_.GetFuture(targetHeight));
    }

    if (1 < client_count_) {
        blocks.emplace_back(block_2_.GetFuture(targetHeight));
        wallets.emplace_back(wallet_2_.GetFuture(targetHeight));
    }

    const auto handle = miner_.Network().Blockchain().GetChain(test_chain_);

    EXPECT_TRUE(handle);

    if (false == handle.IsValid()) { return false; }

    const auto& network = handle.get();
    const auto& headerOracle = network.HeaderOracle();
    auto previousHeader =
        headerOracle.LoadHeader(headerOracle.BestHash(ancestor))->as_Bitcoin();

    for (auto i = 0_uz; i < count; ++i) {
        auto promise = mined_blocks_.allocate();

        OT_ASSERT(gen);

        auto tx = gen(previousHeader.Height() + 1);

        OT_ASSERT(tx);

        auto block = miner_.Factory().BitcoinBlock(
            previousHeader,
            tx,
            previousHeader.nBits(),
            extra,
            previousHeader.Version(),
            [start{ot::Clock::now()}] {
                return (ot::Clock::now() - start) > std::chrono::minutes(2);
            });

        OT_ASSERT(block);

        promise.set_value(block->Header().Hash());
        const auto added = network.AddBlock(block);

        OT_ASSERT(added);

        previousHeader = block->Header().as_Bitcoin();
    }

    auto output = true;
    constexpr auto limit = std::chrono::minutes(5);
    using Status = std::future_status;

    for (auto& future : blocks) {
        OT_ASSERT(future.wait_for(limit) == Status::ready);

        const auto [height, hash] = future.get();

        EXPECT_EQ(hash, previousHeader.Hash());

        output &= (hash == previousHeader.Hash());
    }

    for (auto& future : wallets) {
        OT_ASSERT(future.wait_for(limit) == Status::ready);

        const auto height = future.get();

        EXPECT_EQ(height, targetHeight);

        output &= (height == targetHeight);
    }

    if (output) { height_ = targetHeight; }

    return output;
}

auto Regtest_fixture_base::Shutdown() noexcept -> void
{
    client_2_startup_s_ = std::nullopt;
    client_1_startup_s_ = std::nullopt;
    sync_server_startup_s_ = std::nullopt;
    miner_startup_s_ = std::nullopt;
    wallet_listener_.clear();
    block_listener_.clear();
    mined_block_cache_.reset();
    peer_listener_.reset();
    listen_address_.reset();
}

auto Regtest_fixture_base::Start() noexcept -> bool
{
    const auto miner = [&]() -> std::function<bool()> {
        const auto start = Start(miner_);

        return [=] {
            EXPECT_TRUE(start);

            return start;
        };
    }();
    const auto syncServer = [&]() -> std::function<bool()> {
        const auto start = Start(sync_server_);

        return [=] {
            EXPECT_TRUE(start);

            return start;
        };
    }();
    const auto client1 = [&]() -> std::function<bool()> {
        if (0 < client_count_) {
            const auto start = Start(client_1_);

            return [=] {
                EXPECT_TRUE(start);

                return start;
            };
        } else {

            return [] { return true; };
        }
    }();
    const auto client2 = [&]() -> std::function<bool()> {
        if (1 < client_count_) {
            const auto start = Start(client_2_);

            return [=] {
                EXPECT_TRUE(start);

                return start;
            };
        } else {

            return [] { return true; };
        }
    }();

    return miner() && syncServer() && client1() && client2();
}

auto Regtest_fixture_base::Start(
    const ot::api::session::Client& instance) noexcept -> bool
{
    return instance.Network().Blockchain().Start(test_chain_);
}

auto Regtest_fixture_base::TestUTXOs(
    const Expected& expected,
    const ot::Vector<UTXO>& utxos) const noexcept -> bool
{
    auto out = true;

    for (const auto& utxo : utxos) {
        const auto& [outpoint, pOutput] = utxo;

        EXPECT_TRUE(pOutput);

        if (!pOutput) {
            out = false;

            continue;
        }

        const auto& output = *pOutput;

        try {
            const auto& [exKey, exAmount, exPattern] = expected.at(outpoint);
            out &= (output.Value() == exAmount);

            EXPECT_EQ(output.Value(), exAmount);

            const auto& script = output.Script();
            using Position = ot::blockchain::bitcoin::block::Script::Position;
            out &= (script.Role() == Position::Output);

            EXPECT_EQ(script.Role(), Position::Output);

            const auto data = get_bytes(script);

            EXPECT_TRUE(data.has_value());

            if (false == data.has_value()) {
                out = false;

                continue;
            }

            out &= (data.value() == exKey.Bytes());
            out &= (script.Type() == exPattern);

            EXPECT_EQ(data.value(), exKey.Bytes());
            EXPECT_EQ(script.Type(), exPattern);
        } catch (...) {
            EXPECT_EQ(outpoint.str(), "this will never be true");

            out = false;
        }
    }

    return out;
}

auto Regtest_fixture_base::TestWallet(
    const ot::api::session::Client& api,
    const TXOState& state) const noexcept -> bool
{
    auto output{true};
    const auto handle = api.Network().Blockchain().GetChain(test_chain_);

    EXPECT_TRUE(handle);

    if (false == handle.IsValid()) { return false; }

    const auto& network = handle.get();
    const auto& wallet = network.Wallet();
    using Balance = ot::blockchain::Balance;
    static const auto blankNym = ot::identifier::Nym{};
    static const auto blankAccount = ot::identifier::Generic{};
    static const auto noBalance = Balance{0, 0};
    static const auto blankData = TXOState::Data{};
    const auto test2 = [&](const auto& eBalance,
                           const auto wBalance,
                           const auto nBalance,
                           const auto outpoints) {
        auto output{true};
        output &= (wBalance == eBalance);
        output &= (nBalance == eBalance);
        output &= outpoints;

        EXPECT_EQ(wBalance.first, eBalance.first);
        EXPECT_EQ(wBalance.second, eBalance.second);
        EXPECT_EQ(nBalance.first, eBalance.first);
        EXPECT_EQ(nBalance.second, eBalance.second);
        EXPECT_TRUE(outpoints);

        return output;
    };
    const auto test =
        [&](const auto& eBalance, const auto wBalance, const auto outpoints) {
            return test2(eBalance, wBalance, wBalance, outpoints);
        };
    output &= test2(
        state.wallet_.balance_,
        wallet.GetBalance(),
        network.GetBalance(),
        compare_outpoints(wallet, state.wallet_));
    output &= test2(
        noBalance,
        wallet.GetBalance(blankNym),
        network.GetBalance(blankNym),
        compare_outpoints(wallet, blankNym, blankData));
    output &= test(
        noBalance,
        wallet.GetBalance(blankNym, blankAccount),
        compare_outpoints(wallet, blankNym, blankAccount, blankData));

    for (const auto& [nymID, nymData] : state.nyms_) {
        output &= test2(
            nymData.nym_.balance_,
            wallet.GetBalance(nymID),
            network.GetBalance(nymID),
            compare_outpoints(wallet, nymID, nymData.nym_));
        output &= test(
            noBalance,
            wallet.GetBalance(nymID, blankAccount),
            compare_outpoints(wallet, nymID, blankAccount, blankData));

        for (const auto& [accountID, accountData] : nymData.accounts_) {
            output &= test(
                accountData.balance_,
                wallet.GetBalance(nymID, accountID),
                compare_outpoints(wallet, nymID, accountID, nymData.nym_));
            output &= test(
                noBalance,
                wallet.GetBalance(blankNym, accountID),
                compare_outpoints(wallet, blankNym, accountID, blankData));
        }
    }

    return output;
}
}  // namespace ottest
