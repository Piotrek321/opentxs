// Copyright (c) 2010-2022 The Open-Transactions developers
// // This Source Code Form is subject to the terms of the Mozilla Public
// // License, v. 2.0. If a copy of the MPL was not distributed with this
// // file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/RegtestSimple.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <atomic>
#include <future>
#include <iostream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <tuple>

#include "internal/blockchain/Blockchain.hpp"
#include "internal/util/P0330.hpp"
#include "ottest/fixtures/blockchain/Regtest.hpp"
#include "ottest/fixtures/common/User.hpp"

namespace ottest
{
using namespace opentxs::literals;

RegtestListener::RegtestListener(const ot::api::session::Client& client)
    : block_listener(std::make_unique<BlockListener>(client))
    , wallet_listener(std::make_unique<WalletListener>(client))
{
}

Regtest_fixture_simple::Regtest_fixture_simple()
    : Regtest_fixture_normal(0, ot::Options{}.SetBlockchainStorageLevel(1))
    , send_transactions_()
    , users_()
    , user_listeners_()
    , core_wallet_()
    , auxiliary_wallets_()
{
}

void Regtest_fixture_simple::SetUp()
{
    const std::string name_bob = "Bob";
    const std::string name_chris = "Chris";
    auto chris_words =
        "eight upper indicate swift arch wrestle injury crystal super reward";

    core_wallet_ = {"Alice", GetPaymentCodeVector3().alice_.words_};
    auxiliary_wallets_.emplace(
        name_bob,
        WalletDescription{name_bob, GetPaymentCodeVector3().bob_.words_});
    auxiliary_wallets_.emplace(
        name_chris, WalletDescription{name_chris, chris_words});
}

auto Regtest_fixture_simple::CreateNym(
    const ot::api::session::Client& api,
    const ot::UnallocatedCString& name,
    const ot::UnallocatedCString& seed,
    int index) noexcept -> User&
{
    const auto reason = api.Factory().PasswordPrompt(__func__);
    auto [it, added] = users_.try_emplace(
        name,
        api.Crypto().Seed().Words(seed, reason),
        name,
        api.Crypto().Seed().Passphrase(seed, reason));

    EXPECT_TRUE(added);

    auto& user = it->second;

    return user;
}

auto Regtest_fixture_simple::ImportBip39(
    const ot::api::Session& api,
    const ot::UnallocatedCString& words) const noexcept
    -> ot::UnallocatedCString
{
    using SeedLang = ot::crypto::Language;
    using SeedStyle = ot::crypto::SeedStyle;
    const auto reason = api.Factory().PasswordPrompt(__func__);
    const auto id = api.Crypto().Seed().ImportSeed(
        ot_.Factory().SecretFromText(words),
        ot_.Factory().SecretFromText(""),
        SeedStyle::BIP39,
        SeedLang::en,
        reason);

    return id;
}

auto Regtest_fixture_simple::TransactionGenerator(
    const User& user,
    Height height,
    unsigned count,
    unsigned amount) -> Transaction
{
    using OutputBuilder = ot::api::session::Factory::OutputBuilder;
    using Index = ot::Bip32Index;
    using Subchain = bca::Subchain;

    auto output = ot::UnallocatedVector<OutputBuilder>{};
    auto meta = ot::UnallocatedVector<OutpointMetadata>{};
    meta.reserve(count);

    const auto keys = ot::UnallocatedSet<ot::blockchain::crypto::Key>{};
    const auto baseAmount = ot::blockchain::Amount{amount};

    const auto reason = user.api_->Factory().PasswordPrompt(__func__);
    const auto& account = GetHDAccount(user);

    for (auto i = Index{0}; i < Index{count}; ++i) {
        const auto index = account.Reserve(
            Subchain::External, user.api_->Factory().PasswordPrompt(""));
        const auto& element =
            account.BalanceElement(Subchain::External, index.value_or(0));
        const auto key = element.Key();

        const auto& [bytes, value, pattern] = meta.emplace_back(
            element.PubkeyHash(), baseAmount, Pattern::PayToPubkeyHash);
        output.emplace_back(
            value,
            miner_.Factory().BitcoinScriptP2PKH(test_chain_, *key),
            keys);
    }

    auto output_transaction = miner_.Factory().BitcoinGenerationTransaction(
        test_chain_, height, std::move(output), coinbase_fun_);

    const auto& txid =
        transactions_ptxid_.emplace_back(output_transaction->ID()).get();

    for (auto i = Index{0}; i < Index{count}; ++i) {
        auto& [bytes, amount, pattern] = meta.at(i);
        expected_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(txid.Bytes(), i),
            std::forward_as_tuple(
                std::move(bytes), std::move(amount), std::move(pattern)));
    }

    return output_transaction;
}

auto Regtest_fixture_simple::MineBlocks(
    const Height ancestor,
    const std::size_t count) noexcept -> bool
{
    auto target = ancestor + static_cast<int>(count);
    auto blocks = ot::UnallocatedVector<BlockListener::Future>{};
    auto wallets = ot::UnallocatedVector<WalletListener::Future>{};
    blocks.reserve(users_.size());
    wallets.reserve(users_.size());

    for (auto& listeners : user_listeners_) {
        blocks.emplace_back(listeners.second.block_listener->GetFuture(target));
        wallets.emplace_back(
            listeners.second.wallet_listener->GetFuture(target));
    }

    auto success = Mine(ancestor, count);

    for (auto& future : blocks) {
        EXPECT_TRUE(
            future.wait_for(wait_time_limit_) == std::future_status::ready);
    }

    for (auto& future : wallets) {
        EXPECT_TRUE(
            future.wait_for(wait_time_limit_) == std::future_status::ready);
    }

    return success;
}

auto Regtest_fixture_simple::MineBlocks(
    const User& user,
    Height ancestor,
    unsigned block_number,
    unsigned transaction_number,
    unsigned amount) noexcept
    -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Header>
{
    auto target = ancestor + block_number;
    auto blocks = ot::UnallocatedVector<BlockListener::Future>{};
    auto wallets = ot::UnallocatedVector<WalletListener::Future>{};
    blocks.reserve(users_.size());
    wallets.reserve(users_.size());

    for (auto& listeners : user_listeners_) {
        blocks.emplace_back(listeners.second.block_listener->GetFuture(target));
        wallets.emplace_back(
            listeners.second.wallet_listener->GetFuture(target));
    }
    Generator gen = [&](Height height) -> Transaction {
        return TransactionGenerator(user, height, transaction_number, amount);
    };
    auto mined_header = MineBlocks(ancestor, block_number, gen, {});
    for (auto& future : blocks) {
        EXPECT_TRUE(
            future.wait_for(wait_time_limit_) == std::future_status::ready);

        const auto [height, hash] = future.get();
        EXPECT_EQ(hash, mined_header->Hash());
    }
    for (auto& future : wallets) {
        EXPECT_TRUE(
            future.wait_for(wait_time_limit_) == std::future_status::ready);
    }
    return mined_header;
}

auto Regtest_fixture_simple::MineBlocks(
    Height ancestor,
    std::size_t block_number,
    const Generator& gen,
    const ot::UnallocatedVector<Transaction>& extra) noexcept
    -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Header>
{
    const auto& network = miner_.Network().Blockchain().GetChain(test_chain_);
    const auto& headerOracle = network.HeaderOracle();
    auto previousHeader =
        headerOracle.LoadHeader(headerOracle.BestHash(ancestor))->as_Bitcoin();

    for (auto i = 0_uz; i < block_number; ++i) {
        EXPECT_TRUE(gen);

        auto tx = gen(previousHeader.Height() + 1);

        auto block = miner_.Factory().BitcoinBlock(
            previousHeader,
            tx,
            previousHeader.nBits(),
            extra,
            previousHeader.Version(),
            [start{ot::Clock::now()}] {
                return (ot::Clock::now() - start) > std::chrono::minutes(2);
            });

        EXPECT_TRUE(block);

        const auto added = network.AddBlock(block);

        EXPECT_TRUE(added);

        previousHeader = block->Header().as_Bitcoin();
    }

    return std::make_unique<opentxs::blockchain::bitcoin::block::Header>(
        previousHeader);
}

auto Regtest_fixture_simple::CreateClient(
    ot::Options client_args,
    int instance,
    const ot::UnallocatedCString& name,
    const ot::UnallocatedCString& words,
    const b::p2p::Address& address) -> std::pair<User&, bool>
{
    const auto& client = ot_.StartClientSession(client_args, instance);

    const auto start = client.Network().Blockchain().Start(test_chain_);

    const auto& network = client.Network().Blockchain().GetChain(test_chain_);
    const auto added = network.AddPeer(address);

    auto seed = ImportBip39(client, words);
    const auto& user = CreateNym(client, name, seed, instance);

    auto cb = [](User& user) {
        const auto& api = *user.api_;
        const auto& nymID = user.nym_id_.get();
        const auto reason = api.Factory().PasswordPrompt(__func__);
        api.Crypto().Blockchain().NewHDSubaccount(
            nymID,
            opentxs::blockchain::crypto::HDProtocol::BIP_44,
            test_chain_,
            reason);
    };

    user.init_custom(client, cb);

    client.UI().AccountActivity(
        user.nym_id_, GetHDAccount(user).Parent().AccountID(), []() {});
    client.UI().AccountList(user.nym_id_, []() {});

    const auto [it, listener_added] = user_listeners_.emplace(name, client);

    std::promise<void> promise;
    std::future<void> done = promise.get_future();
    auto cb_connected = [&](zmq::Message&& msg, std::atomic_int& counter) {
        promise.set_value();
    };
    std::atomic_int client_peers;
    ot::OTZMQListenCallback client_cb_(
        ot::network::zeromq::ListenCallback::Factory(
            [&](auto&& msg) { cb_connected(std::move(msg), client_peers); }));
    ot::OTZMQSubscribeSocket client_socket(
        user.api_->Network().ZeroMQ().SubscribeSocket(client_cb_));
    if (!client_socket->Start(
            (wait_for_handshake_
                 ? user.api_->Endpoints().BlockchainPeer()
                 : user.api_->Endpoints().BlockchainPeerConnection())
                .data())) {
        throw std::runtime_error("Error connecting to client1 socket");
    }

    const auto status = done.wait_for(std::chrono::seconds(120));
    const auto future = (std::future_status::ready == status);

    return {user, added && start && future && listener_added};
}

auto Regtest_fixture_simple::CloseClient(const ot::UnallocatedCString& name)
    -> void
{
    users_.at(name).api_->Network().Blockchain().Stop(test_chain_);
    users_.erase(name);
    user_listeners_.erase(name);
}

auto Regtest_fixture_simple::GetBalance(const User& user) const -> const Amount
{
    const auto& account = GetHDAccount(user);
    const auto& id = account.Parent().AccountID();
    const auto& widget = user.api_->UI().AccountActivity(user.nym_id_, id);
    return widget.Balance();
}

auto Regtest_fixture_simple::GetDisplayBalance(const User& user) const
    -> const ot::UnallocatedCString
{
    const auto& account = GetHDAccount(user);
    const auto& id = account.Parent().AccountID();
    const auto& widget = user.api_->UI().AccountActivity(user.nym_id_, id);
    return widget.DisplayBalance();
}

auto Regtest_fixture_simple::GetSyncProgress(const User& user) const
    -> const std::pair<int, int>
{
    const auto& account = GetHDAccount(user);
    const auto& id = account.Parent().AccountID();
    const auto& widget = user.api_->UI().AccountActivity(user.nym_id_, id);
    return widget.SyncProgress();
}

auto Regtest_fixture_simple::GetSyncPercentage(const User& user) const -> double
{
    const auto& account = GetHDAccount(user);
    const auto& id = account.Parent().AccountID();
    const auto& widget = user.api_->UI().AccountActivity(user.nym_id_, id);
    return widget.SyncPercentage();
}

auto Regtest_fixture_simple::GetHDAccount(const User& user) const noexcept
    -> const bca::HD&
{
    return user.api_->Crypto()
        .Blockchain()
        .Account(user.nym_id_, test_chain_)
        .GetHD()
        .at(0);
}

auto Regtest_fixture_simple::GetNextBlockchainAddress(const User& user)
    -> const ot::UnallocatedCString
{
    const auto& account = GetHDAccount(user);
    const auto index = account.Reserve(
        Subchain::External, user.api_->Factory().PasswordPrompt(""));
    const auto& element =
        account.BalanceElement(Subchain::External, index.value_or(0));

    return element.Address(opentxs::blockchain::crypto::AddressStyle::P2PKH);
}

auto Regtest_fixture_simple::WaitForSynchro(
    const User& user,
    const Height target,
    const Amount expected_balance,
    std::chrono::seconds max_duration) -> void
{
    if (expected_balance == 0) { return; }

    auto begin = std::chrono::steady_clock::now();
    auto now = begin;
    auto end = begin + (max_duration == 0s ? wait_time_limit_ : max_duration);

    while (now < end) {
        now = std::chrono::steady_clock::now();
        auto progress = GetSyncProgress(user);
        auto balance = GetBalance(user);
        std::ostringstream percentage;
        percentage.precision(2);
        percentage << std::fixed << GetSyncPercentage(user);

        ot::LogConsole()(
            "Waiting for synchronization, balance: " + GetDisplayBalance(user) +
            ", expected balance: " + GetDisplayBalance(expected_balance) +
            ", sync percentage: " + percentage.str() + "%, sync progress [" +
            std::to_string(progress.first) + "," +
            std::to_string(progress.second) + "]" +
            ", target height: " + std::to_string(target))
            .Flush();
        if (progress.first == target && progress.second == target &&
            (balance == expected_balance)) {
            ot::LogConsole()(
                "Client synchronized in " +
                std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                   now - begin)
                                   .count()) +
                " seconds")
                .Flush();
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    now = std::chrono::steady_clock::now();
    auto remaining_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - end)
            .count();
    EXPECT_LT(remaining_ms, 0);
}
auto Regtest_fixture_simple::GetDisplayBalance(
    opentxs::Amount value) const noexcept -> std::string
{
    return opentxs::blockchain::internal::Format(test_chain_, value);
}

auto Regtest_fixture_simple::GetWalletAddress(const User& user) const noexcept
    -> std::string
{
    return GetHDAccount(user)
        .BalanceElement(opentxs::blockchain::crypto::Subchain::Internal, 0)
        .Address(opentxs::blockchain::crypto::AddressStyle::P2PKH);
}

auto Regtest_fixture_simple::GetWalletName(const User& user) const noexcept
    -> std::string
{
    return user.nym_->Alias();
}

auto Regtest_fixture_simple::GetTransactions(const User& user) const noexcept
    -> opentxs::UnallocatedVector<opentxs::blockchain::block::pTxid>
{
    const auto& network =
        user.api_->Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();

    return wallet.GetTransactions();
}

void Regtest_fixture_simple::MineTransaction(
    const User& user,
    const opentxs::blockchain::block::pTxid& transactions_to_confirm,
    Height& current_height)
{
    std::vector<Transaction> transactions;

    auto send_transaction =
        user.api_->Crypto().Blockchain().LoadTransactionBitcoin(
            transactions_to_confirm);
    if (!send_transaction) { return; }
    transactions_ptxid_.emplace_back(send_transaction->ID());
    transactions.emplace_back(std::move(send_transaction));

    Mine(current_height++, transactions.size(), default_, transactions);

    send_transactions_.emplace_back(std::move(transactions[0]));
}

void Regtest_fixture_simple::SendCoins(
    const User& receiver,
    const User& sender,
    Height& current_height,
    const int coins_to_send)
{
    const auto& network =
        sender.api_->Network().Blockchain().GetChain(test_chain_);

    const ot::UnallocatedCString memo_outgoing = "test";

    const auto address = GetNextBlockchainAddress(receiver);

    std::cout << sender.name_ + " send " << coins_to_send << " to "
              << receiver.name_ << " address: " << address << std::endl;

    const auto& network_receiver =
        receiver.api_->Network().Blockchain().GetChain(test_chain_);
    auto output_size =
        network_receiver.Wallet()
            .GetOutputs(
                receiver.nym_->ID(), blockchain::node::TxoState::ConfirmedNew)
            .size();
    auto future = network.SendToAddress(
        sender.nym_id_, address, coins_to_send, memo_outgoing);
    // We need to confirm send transaction, so it is available after restore
    MineTransaction(sender, future.get().second, current_height);
    WaitForOutputs(receiver, send_transactions_, output_size);
}

auto Regtest_fixture_simple::WaitForOutputs(
    const User& receiver,
    std::vector<Transaction>& transactions,
    const size_t output_size) -> void
{
    const auto& network =
        receiver.api_->Network().Blockchain().GetChain(test_chain_);

    auto begin = std::chrono::steady_clock::now();
    auto now = begin;
    auto end = begin + wait_time_limit_;

    for (auto& transaction : transactions) {
        while (now < end) {
            auto wallet_transactions = network.Wallet().GetTransactions();

            if (std::find(
                    wallet_transactions.begin(),
                    wallet_transactions.end(),
                    transaction->ID()) != wallet_transactions.end()) {

                auto wallet_outputs = network.Wallet().GetOutputs(
                    receiver.nym_->ID(),
                    blockchain::node::TxoState::ConfirmedNew);

                if (output_size + 1 == wallet_outputs.size()) {
                    ot::LogConsole()("WaitForOutputs found all outputs ")(
                        wallet_outputs.size())
                        .Flush();
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    now = std::chrono::steady_clock::now();
    EXPECT_LT(now, end);
}

Amount Regtest_fixture_simple::CalculateFee(
    const std::vector<Transaction>& send_transactions,
    std::vector<std::unique_ptr<
        const opentxs::blockchain::bitcoin::block::Transaction>>&
        loaded_transactions) const
{
    // TODO is there a way to get all transaction from blockchain/wallet
    // etc? Because currently we base on our test fixture

    Amount outputs_value, inputs_value, fee;
    for (const auto& send_transaction : send_transactions) {
        for (const auto& output : send_transaction->Outputs())
            outputs_value += output.Value();

        for (std::size_t i = 0; i < send_transaction->Inputs().size(); ++i) {
            auto previousOutput =
                send_transaction->Inputs().at(i).PreviousOutput();

            auto& output =
                loaded_transactions[i]->Outputs().at(previousOutput.Index());
            inputs_value += output.Value();
        }
    }

    fee = inputs_value - outputs_value;
    ot::LogConsole()("Calculated fee: " + GetDisplayBalance(fee)).Flush();

    return fee;
}
std::vector<
    std::unique_ptr<const opentxs::blockchain::bitcoin::block::Transaction>>
Regtest_fixture_simple::CollectTransactionsForFeeCalculations(
    const User& user,
    const std::vector<Transaction>& send_transactions,
    const Transactions& all_transactions) const
{
    std::vector<
        std::unique_ptr<const opentxs::blockchain::bitcoin::block::Transaction>>
        loaded_transactions;

    for (const auto& send_transaction : send_transactions) {
        for (const auto& input : send_transaction->Inputs()) {
            auto prevOutTrxId = input.PreviousOutput().TxidAsHex();

            auto iter = std::find_if(
                all_transactions.begin(),
                all_transactions.end(),
                [&prevOutTrxId](const auto& transaction) {
                    return transaction->asHex() == prevOutTrxId;
                });

            if (iter != all_transactions.end()) {
                auto loaded_transaction =
                    user.api_->Crypto().Blockchain().LoadTransactionBitcoin(
                        prevOutTrxId);

                if (!loaded_transaction)
                    throw std::runtime_error("Cannot load transaction");
                loaded_transactions.push_back(std::move(loaded_transaction));

            } else
                ot::LogConsole()(
                    "No matching transaction found in all transaction for "
                    "output id: " +
                    prevOutTrxId)
                    .Flush();
        }
    }
    return loaded_transactions;
}

void Regtest_fixture_simple::CollectOutputs(
    const User& user,
    OutputsSet& all_outputs,
    std::map<TxoState, std::size_t>& number_of_outputs_per_type) const
{
    const auto& network =
        user.api_->Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();

    auto all_types = {
        TxoState::Immature,
        TxoState::ConfirmedSpend,
        TxoState::UnconfirmedSpend,
        TxoState::ConfirmedNew,
        TxoState::UnconfirmedNew,
        TxoState::OrphanedSpend,
        TxoState::OrphanedNew};
    for (const auto& type : all_types) {
        number_of_outputs_per_type[type] = wallet.GetOutputs(type).size();
    }

    for (auto& output : wallet.GetOutputs(TxoState::All))
        all_outputs.insert(std::move(output));
}

void Regtest_fixture_simple::ValidateOutputs(
    const User& user,
    const std::set<UTXO>& outputs_to_compare,
    const std::map<TxoState, std::size_t>&
        number_of_outputs_per_type_to_compare) const
{
    std::set<UTXO> outputs;
    std::map<TxoState, std::size_t> number_of_outputs_per_type;

    CollectOutputs(user, outputs, number_of_outputs_per_type);

    EXPECT_EQ(
        number_of_outputs_per_type_to_compare, number_of_outputs_per_type);

    EXPECT_EQ(outputs_to_compare.size(), outputs.size());

    std::cout << "Comparing outputs\n";
    compare_outputs(outputs_to_compare, outputs);
    std::cout << "Comparison finished.\n";
}

void Regtest_fixture_simple::MineBlocksForUsers(
    std::vector<std::reference_wrapper<const User>>& users,
    Height& target_height,
    const int& number_of_blocks_to_mine)
{
    for (const auto& user : users) {
        MineBlocks(
            user,
            target_height,
            number_of_blocks_to_mine,
            transaction_in_block_,
            amount_in_transaction_);
        target_height += number_of_blocks_to_mine;
    }
    advance_blockchain(users, target_height);
}
auto Regtest_fixture_simple::GetHeight(const User& user) const noexcept
    -> opentxs::blockchain::block::Height
{
    const auto& network =
        user.api_->Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();

    return wallet.Height();
}

void Regtest_fixture_simple::compare_outputs(
    const std::set<UTXO>& pre_reboot_outputs,
    const std::set<UTXO>& post_reboot_outputs) const
{
    auto iter = post_reboot_outputs.begin();
    for (const auto& [outpoint, output] : pre_reboot_outputs) {
        EXPECT_EQ(iter->first, outpoint);
        EXPECT_EQ(iter->second->Value(), output->Value());
        EXPECT_TRUE(
            iter->second->Script().CompareScriptElements(output->Script()));
        iter++;
    }
}

void Regtest_fixture_simple::advance_blockchain(
    std::vector<std::reference_wrapper<const User>>& users,
    Height& target_height)
{
    std::vector<std::unique_ptr<ScanListener>> scan_listeners;
    std::vector<std::unique_ptr<std::future<void>>> external_scan_listeners;
    std::vector<std::unique_ptr<std::future<void>>> internal_scan_listeners;

    for (const auto& user : users) {
        auto scan_listener = std::make_unique<ScanListener>(*user.get().api_);
        scan_listeners.push_back(std::move(scan_listener));
    }

    auto height = target_height + static_cast<int>(MaturationInterval());

    for (std::size_t i = 0; i < users.size(); ++i) {
        external_scan_listeners.push_back(
            std::make_unique<std::future<void>>(scan_listeners[i]->get_future(
                GetHDAccount(users[i]), bca::Subchain::External, height)));
        internal_scan_listeners.push_back(
            std::make_unique<std::future<void>>(scan_listeners[i]->get_future(
                GetHDAccount(users[i]), bca::Subchain::Internal, height)));
    }

    MineBlocks(target_height, static_cast<int>(MaturationInterval()));

    for (std::size_t i = 0; i < users.size(); ++i) {
        EXPECT_TRUE(scan_listeners[i]->wait(*external_scan_listeners[i]));
        EXPECT_TRUE(scan_listeners[i]->wait(*internal_scan_listeners[i]));
    }

    target_height += static_cast<int>(MaturationInterval());
}

void Regtest_fixture_round_robin::SetUp()
{
    Regtest_fixture_simple::SetUp();

    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());

    auto [user_core, success_core] = CreateClient(
        opentxs::Options{},
        instance_++,
        core_wallet_.name_,
        core_wallet_.words_,
        address_);
    EXPECT_TRUE(success_core);

    for (auto& entry : auxiliary_wallets_) {
        auto& user_to_create = entry.second;
        auto [user, success] = CreateClient(
            opentxs::Options{},
            instance_++,
            user_to_create.name_,
            user_to_create.words_,
            address_);
        EXPECT_TRUE(success);
    }
}
}  // namespace ottest
