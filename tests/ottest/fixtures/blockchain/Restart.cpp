// Copyright (c) 2010-2022 The Open-Transactions developers
// // This Source Code Form is subject to the terms of the Mozilla Public
// // License, v. 2.0. If a copy of the MPL was not distributed with this
// // file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/RegtestSimple.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <ostream>
#include <string_view>
#include <tuple>

#include "ottest/fixtures/blockchain/Restart.hpp"
#include "ottest/fixtures/common/User.hpp"

namespace ottest
{
const User& Restart_fixture::createUser(
    const std::string& name,
    const std::string& words)
{
    auto [user_alice, success_alice] =
        CreateClient(opentxs::Options{}, instance++, name, words, address_);
    EXPECT_TRUE(success_alice);

    return user_alice;
}

const User& Restart_fixture::createSenderAlice()
{
    return createUser(name_alice, words_alice);
}

const User& Restart_fixture::createReceiverBob()
{
    return createUser(name_bob, words_bob);
}

void Restart_fixture::mineForBothUsers(
    const User& user_bob,
    const User& user_alice)
{
    Height begin = targetHeight;
    auto scan_listener_alice = std::make_unique<ScanListener>(*user_alice.api_);
    auto scan_listener_bob = std::make_unique<ScanListener>(*user_bob.api_);

    targetHeight += blocks_number_;

    std::cout << "Mine for alice\n";
    // mine coin for Alice
    MineBlocks(
        user_alice,
        begin,
        blocks_number_,
        transaction_in_block_,
        amount_in_transaction_);

    begin += blocks_number_;
    targetHeight += blocks_number_;

    std::cout << "Mine for bob\n";
    // mine coin for Bob
    MineBlocks(
        user_bob,
        begin,
        blocks_number_,
        transaction_in_block_,
        amount_in_transaction_);

    begin += blocks_number_;
    targetHeight += static_cast<int>(MaturationInterval()) + 1;

    auto scan_listener_external_alice_f = scan_listener_alice->get_future(
        GetHDAccount(user_alice), bca::Subchain::External, targetHeight);
    auto scan_listener_internal_alice_f = scan_listener_alice->get_future(
        GetHDAccount(user_alice), bca::Subchain::Internal, targetHeight);
    auto scan_listener_external_bob_f = scan_listener_bob->get_future(
        GetHDAccount(user_bob), bca::Subchain::External, targetHeight);
    auto scan_listener_internal_bob_f = scan_listener_bob->get_future(
        GetHDAccount(user_bob), bca::Subchain::Internal, targetHeight);

    std::cout << "Mine targetHeight: " << targetHeight << std::endl;
    // mine MaturationInterval number block with
    MineBlocks(begin, static_cast<int>(MaturationInterval()) + 1);

    EXPECT_TRUE(scan_listener_alice->wait(scan_listener_external_alice_f));
    EXPECT_TRUE(scan_listener_alice->wait(scan_listener_internal_alice_f));

    EXPECT_EQ(
        GetBalance(user_alice),
        amount_in_transaction_ * blocks_number_ * transaction_in_block_);

    EXPECT_TRUE(scan_listener_bob->wait(scan_listener_external_bob_f));
    EXPECT_TRUE(scan_listener_bob->wait(scan_listener_internal_bob_f));
    EXPECT_EQ(
        GetBalance(user_bob),
        amount_in_transaction_ * blocks_number_ * transaction_in_block_);
    EXPECT_EQ(GetTransactions(user_bob).size(), 1);
    EXPECT_EQ(GetTransactions(user_alice).size(), 1);
}

void Restart_fixture::mineTransaction(
    const User& user,
    const opentxs::blockchain::block::pTxid& transactionToConfirm)
{
    std::vector<Transaction> vec;

    auto send_transaction =
        user.api_->Crypto().Blockchain().LoadTransactionBitcoin(
            transactionToConfirm);
    vec.emplace_back(std::move(send_transaction));

    Mine(targetHeight++, vec.size(), default_, vec);
}

void Restart_fixture::sendCoins(const User& receiver, const User& sender)
{
    const auto& network =
        sender.api_->Network().Blockchain().GetChain(test_chain_);

    const ot::UnallocatedCString memo_outgoing = "test";

    const auto address = GetNextBlockchainAddress(receiver);

    std::cout << sender.name_ + " send " << coin_to_send_ << " to "
              << receiver.name_ << " address: " << address << std::endl;

    auto future = network.SendToAddress(
        sender.nym_id_, address, coin_to_send_, memo_outgoing);

    // We need to confirm send transaction, so it is available after restore
    mineTransaction(sender, future.get().second);
}

void Restart_fixture::collectOutputs(
    const User& user_bob,
    const User& user_alice,
    Vector<UTXO>& all_bobs_outputs,
    Vector<UTXO>& all_alice_outputs,
    std::map<TxoState, std::size_t>& number_of_outputs_per_type_bob,
    std::map<TxoState, std::size_t>& number_of_outputs_per_type_alice)
{
    const auto& bobs_network =
        user_bob.api_->Network().Blockchain().GetChain(test_chain_);
    const auto& bobs_wallet = bobs_network.Wallet();

    const auto& alice_network =
        user_alice.api_->Network().Blockchain().GetChain(test_chain_);
    const auto& alice_wallet = alice_network.Wallet();

    all_bobs_outputs = bobs_wallet.GetOutputs(TxoState::All);
    all_alice_outputs = alice_wallet.GetOutputs(TxoState::All);

    auto all_types = {
        TxoState::Immature,
        TxoState::ConfirmedSpend,
        TxoState::UnconfirmedSpend,
        TxoState::ConfirmedNew,
        TxoState::UnconfirmedNew,
        TxoState::OrphanedSpend,
        TxoState::OrphanedNew};
    for (const auto& type : all_types) {
        number_of_outputs_per_type_bob[type] =
            bobs_wallet.GetOutputs(type).size();
        number_of_outputs_per_type_alice[type] =
            alice_wallet.GetOutputs(type).size();
    }
}

void Restart_fixture::collectOutputsAsSet(
    const User& user_bob,
    const User& user_alice,
    OutputsSet& all_bobs_outputs,
    OutputsSet& all_alice_outputs,
    std::map<TxoState, std::size_t>& number_of_outputs_per_type_bob,
    std::map<TxoState, std::size_t>& number_of_outputs_per_type_alice)
{
    // Wait for outputs to process
    sleep(20);

    Vector<UTXO> bobs_outputs;
    Vector<UTXO> alice_outputs;

    collectOutputs(
        user_bob,
        user_alice,
        bobs_outputs,
        alice_outputs,
        number_of_outputs_per_type_bob,
        number_of_outputs_per_type_alice);

    EXPECT_EQ(bobs_outputs.size(), bobs_outputs.size());
    EXPECT_EQ(alice_outputs.size(), alice_outputs.size());

    for (std::size_t i = 0; i < bobs_outputs.size(); ++i)
        all_bobs_outputs.insert(std::move(bobs_outputs[i]));

    for (std::size_t i = 0; i < alice_outputs.size(); ++i)
        all_alice_outputs.insert(std::move(alice_outputs[i]));
}

std::pair<ot::Amount, ot::Amount> Restart_fixture::collectFeeRate(
    const User& user_bob,
    const User& user_alice)
{
    const auto& bobs_network =
        user_bob.api_->Network().Blockchain().GetChain(test_chain_);

    const auto& alice_network =
        user_alice.api_->Network().Blockchain().GetChain(test_chain_);

    return std::make_pair(
        bobs_network.Internal().FeeRate(), alice_network.Internal().FeeRate());
}

void Restart_fixture::validateOutputs(
    const User& user_bob_after_reboot,
    const User& user_alice_after_reboot,
    const std::set<UTXO>& bob_outputs,
    const std::set<UTXO>& alice_outputs,
    const std::map<TxoState, std::size_t>& bob_all_outputs_size,
    const std::map<TxoState, std::size_t>& alice_all_outputs_size)
{
    std::set<UTXO> bob_outputs_after_reboot;
    std::set<UTXO> alice_outputs_after_reboot;
    std::map<TxoState, std::size_t> bob_all_outputs_size_after_reboot;
    std::map<TxoState, std::size_t> alice_all_outputs_size_after_reboot;

    collectOutputsAsSet(
        user_bob_after_reboot,
        user_alice_after_reboot,
        bob_outputs_after_reboot,
        alice_outputs_after_reboot,
        bob_all_outputs_size_after_reboot,
        alice_all_outputs_size_after_reboot);

    EXPECT_EQ(bob_all_outputs_size_after_reboot, bob_all_outputs_size);
    EXPECT_EQ(alice_all_outputs_size_after_reboot, alice_all_outputs_size);

    EXPECT_EQ(bob_outputs_after_reboot.size(), bob_outputs.size());
    EXPECT_EQ(alice_outputs_after_reboot.size(), alice_outputs.size());

    std::cout << "Comparing bob outputs\n";
    compareOutputs(bob_outputs, bob_outputs_after_reboot);
    std::cout << "Comparison finished.\nComparing alice outputs\n";
    compareOutputs(alice_outputs, alice_outputs_after_reboot);
    std::cout << "Comparison finished.\n";
}

void Restart_fixture::compareOutputs(
    const std::set<UTXO>& pre_reboot_outputs,
    const std::set<UTXO>& post_reboot_outputs)
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
}  // namespace ottest
