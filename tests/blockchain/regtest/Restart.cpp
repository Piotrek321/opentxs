// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.ko

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/block/Outpoint.hpp"
#include "ottest/fixtures/blockchain/Restart.hpp"
#include "ottest/fixtures/ui/AccountTree.hpp"

namespace ottest
{

TEST_F(Restart_fixture, send_to_client_reboot_confirm_data)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());
    ot::LogConsole()("send_to_client_reboot_confirm_data start").Flush();
    ot::Amount receiver_balance_after_send{};
    ot::Amount sender_balance_after_send{};

    // Create wallets
    const auto& user_alice = CreateUser(name_alice_, words_alice_);
    const auto& user_bob = CreateUser(name_bob_, words_bob_);

    // Get data for later validation
    const auto bobs_payment_code = user_bob.PaymentCode();
    const auto bobs_hd_name = GetHDAccount(user_bob).Name();
    const auto alice_payment_code = user_alice.PaymentCode();
    const auto alice_hd_name = GetHDAccount(user_alice).Name();
    auto bob_address = GetWalletAddress(user_bob);
    auto alice_address = GetWalletAddress(user_alice);

    EXPECT_EQ(GetWalletName(user_bob), name_bob_);
    EXPECT_EQ(GetWalletName(user_alice), name_alice_);

    EXPECT_EQ(0, GetBalance(user_alice));
    EXPECT_EQ(0, GetBalance(user_bob));

    std::vector<std::reference_wrapper<const User>> users{user_bob, user_alice};
    // Mine initial balance
    MineBlocksForUsers(users, current_height_, blocks_number_);

    EXPECT_EQ(balance_after_mine_, GetBalance(user_alice));
    EXPECT_EQ(balance_after_mine_, GetBalance(user_bob));

    // Send coins from alice to bob
    SendCoins(user_bob, user_alice, current_height_);
    WaitForSynchro(
        user_bob, current_height_, balance_after_mine_ + coin_to_send_);

    // Save current balance and check if is correct
    receiver_balance_after_send = GetBalance(user_bob);

    ot::LogConsole()(
        "Bob balance after send " +
        GetDisplayBalance(receiver_balance_after_send))
        .Flush();

    EXPECT_EQ(balance_after_mine_ + coin_to_send_, receiver_balance_after_send);

    // Collect outputs
    std::set<UTXO> bob_outputs{};
    std::map<ot::blockchain::node::TxoState, std::size_t>
        bob_number_of_outputs_per_type{};
    CollectOutputs(user_bob, bob_outputs, bob_number_of_outputs_per_type);

    std::set<UTXO> alice_outputs{};
    std::map<ot::blockchain::node::TxoState, std::size_t>
        alice_number_of_outputs_per_type{};
    CollectOutputs(user_alice, alice_outputs, alice_number_of_outputs_per_type);

    auto loaded_transactions = CollectTransactionsForFeeCalculations(
        user_alice, send_transactions_, transactions_ptxid_);
    auto fee = CalculateFee(send_transactions_, loaded_transactions);

    WaitForSynchro(
        user_alice,
        current_height_,
        Amount{balance_after_mine_} - Amount{coin_to_send_} - fee);
    sender_balance_after_send = GetBalance(user_alice);
    ot::LogConsole()(
        "QQQQ Alice balance after send " +
        GetDisplayBalance(GetBalance(user_alice)))
        .Flush();
    EXPECT_EQ(
        Amount{balance_after_mine_} - Amount{coin_to_send_} - fee,
        sender_balance_after_send);

    // Cleanup
    CloseClient(user_bob.name_);
    CloseClient(user_alice.name_);

    ot::LogConsole()("Users removed").Flush();

    // Restore both wallets
    const auto& user_alice_after_reboot = CreateUser(name_alice_, words_alice_);
    WaitForSynchro(
        user_alice_after_reboot, current_height_, sender_balance_after_send);

    const auto& user_bob_after_reboot = CreateUser(name_bob_, words_bob_);
    WaitForSynchro(
        user_bob_after_reboot, current_height_, receiver_balance_after_send);

    ot::LogConsole()(
        "Bob balance after reboot " + GetDisplayBalance(user_bob_after_reboot))
        .Flush();
    ot::LogConsole()(
        "Alice balance after reboot " +
        GetDisplayBalance(user_alice_after_reboot))
        .Flush();
    // Compare balance
    EXPECT_EQ(GetBalance(user_bob_after_reboot), receiver_balance_after_send);
    EXPECT_EQ(user_bob_after_reboot.PaymentCode(), bobs_payment_code);

    EXPECT_EQ(GetBalance(user_alice_after_reboot), sender_balance_after_send);
    EXPECT_EQ(user_alice_after_reboot.PaymentCode(), alice_payment_code);

    // Compare names and addresses
    EXPECT_EQ(GetWalletName(user_bob_after_reboot), name_bob_);
    EXPECT_EQ(bob_address, GetWalletAddress(user_bob_after_reboot));

    EXPECT_EQ(GetWalletName(user_alice_after_reboot), name_alice_);
    EXPECT_EQ(alice_address, GetWalletAddress(user_alice_after_reboot));

    // Compare wallet name
    EXPECT_EQ(GetHDAccount(user_bob_after_reboot).Name(), bobs_hd_name);
    EXPECT_EQ(GetHDAccount(user_alice_after_reboot).Name(), alice_hd_name);

    EXPECT_EQ(bob_outputs.size(), alice_outputs.size());

    // Compare outputs
    ValidateOutputs(
        user_bob_after_reboot, bob_outputs, bob_number_of_outputs_per_type);

    ValidateOutputs(
        user_alice_after_reboot,
        alice_outputs,
        alice_number_of_outputs_per_type);

    CloseClient(user_bob_after_reboot.name_);
    CloseClient(user_alice_after_reboot.name_);
    ot::LogConsole()("send_to_client_reboot_confirm_data end").Flush();
    Shutdown();
}

}  // namespace ottest
