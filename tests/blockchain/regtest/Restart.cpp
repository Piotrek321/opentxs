// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.ko

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "opentxs/blockchain/block/Outpoint.hpp"
#include "opentxs/blockchain/block/bitcoin/Transaction.hpp"
#include "ottest/fixtures/blockchain/Restart.hpp"
#include "ottest/fixtures/ui/AccountTree.hpp"

namespace ottest
{

TEST_F(Restart_fixture, start_chains_and_connect)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());
}

TEST_F(Restart_fixture, send_to_client_reboot_confirm_data)
{
    ot::LogConsole()("send_to_client_reboot_confirm_data start").Flush();
    ot::Amount receiver_balance_after_send, sender_balance_after_send;

    // Create wallets
    const auto& user_alice = createSenderAlice();
    const auto& user_bob = createReceiverBob();

    // Get data for later validation
    const auto bobs_payment_code = user_bob.PaymentCode();
    const auto bobs_hd_name = GetHDAccount(user_bob).Name();
    const auto alice_payment_code = user_alice.PaymentCode();
    const auto alice_hd_name = GetHDAccount(user_alice).Name();
    auto bob_address = GetWalletAddress(user_bob);
    auto alice_address = GetWalletAddress(user_alice);

    EXPECT_EQ(GetWalletName(user_bob), name_bob);
    EXPECT_EQ(GetWalletName(user_alice), name_alice);

    EXPECT_EQ(0, GetBalance(user_alice));
    EXPECT_EQ(0, GetBalance(user_bob));

    // Mine initial balance
    mineForBothUsers(user_bob, user_alice);

    EXPECT_EQ(balance_after_mine, GetBalance(user_alice));
    EXPECT_EQ(balance_after_mine, GetBalance(user_bob));

    // Send coins from alice to bob
    sendCoins(user_bob, user_alice);

    // Save current balance and check if is correct
    receiver_balance_after_send = GetBalance(user_bob);
    sender_balance_after_send = GetBalance(user_alice);

    ot::LogConsole()(
        "Bob balance after send " +
        GetDisplayBalance(receiver_balance_after_send))
        .Flush();
    ot::LogConsole()(
        "Alice balance after send " +
        GetDisplayBalance(sender_balance_after_send))
        .Flush();

    // Collect outputs
    std::set<UTXO> bob_outputs;
    std::set<UTXO> alice_outputs;
    std::map<ot::blockchain::node::TxoState, std::size_t> bob_all_outputs_size;
    std::map<ot::blockchain::node::TxoState, std::size_t>
        alice_all_outputs_size;

    collectOutputsAsSet(
        user_bob,
        user_alice,
        bob_outputs,
        alice_outputs,
        bob_all_outputs_size,
        alice_all_outputs_size);

    // Collect fees
    auto fee_rates = collectFeeRate(user_bob, user_alice);

    EXPECT_EQ(balance_after_mine + coin_to_send_, receiver_balance_after_send);

    EXPECT_GT(
        sender_balance_after_send,
        (ot::Amount(balance_after_mine) - ot::Amount(coin_to_send_) -
         fee_rates.second));
    EXPECT_LT(sender_balance_after_send, balance_after_mine);

    // Cleanup
    CloseClient(user_bob.name_);
    CloseClient(user_alice.name_);

    ot::LogConsole()("Users removed").Flush();

    // Restore both wallets
    const auto& user_alice_after_reboot = createSenderAlice();
    WaitForSynchro(
        user_alice_after_reboot, targetHeight, sender_balance_after_send);

    const auto& user_bob_after_reboot = createReceiverBob();
    WaitForSynchro(
        user_bob_after_reboot, targetHeight, receiver_balance_after_send);

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
    EXPECT_EQ(GetWalletName(user_bob_after_reboot), name_bob);
    EXPECT_EQ(bob_address, GetWalletAddress(user_bob_after_reboot));

    EXPECT_EQ(GetWalletName(user_alice_after_reboot), name_alice);
    EXPECT_EQ(alice_address, GetWalletAddress(user_alice_after_reboot));

    // Compare fee rates
    auto fee_rates_after_reboot =
        collectFeeRate(user_alice_after_reboot, user_bob_after_reboot);

    EXPECT_EQ(fee_rates_after_reboot, fee_rates);

    // Compare wallet name
    EXPECT_EQ(GetHDAccount(user_bob_after_reboot).Name(), bobs_hd_name);
    EXPECT_EQ(GetHDAccount(user_alice_after_reboot).Name(), alice_hd_name);

    // Compare outputs
    validateOutputs(
        user_bob_after_reboot,
        user_alice_after_reboot,
        bob_outputs,
        alice_outputs,
        bob_all_outputs_size,
        alice_all_outputs_size);

    CloseClient(user_bob_after_reboot.name_);
    CloseClient(user_alice_after_reboot.name_);
    ot::LogConsole()("send_to_client_reboot_confirm_data end").Flush();
}

TEST_F(Restart_fixture, end) { Shutdown(); }

}  // namespace ottest
