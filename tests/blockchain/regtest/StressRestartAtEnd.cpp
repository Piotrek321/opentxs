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
#include "ottest/fixtures/ui/ContactList.hpp"

namespace ottest
{
const int number_of_tests = 10;

TEST_F(Restart_fixture, start_chains_and_connect)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());
}

TEST_F(Restart_fixture, send_multiple_transactions_remove_user_compare)
{
    ot::LogConsole()("send_multiple_transactions_remove_user_compare start")
        .Flush();
    ot::Amount receiver_balance, sender_balance;
    // Create wallets

    const auto& user_alice = createSenderAlice();
    const auto& user_bob = createReceiverBob();

    mineForBothUsers(user_bob, user_alice);

    receiver_balance = GetBalance(user_bob);
    sender_balance = GetBalance(user_alice);

    for (int i = 1; i < number_of_tests; ++i) {
        ot::LogConsole()("iteration no: " + std::to_string(i)).Flush();

        // Send coins from alice to bob
        sendCoins(user_bob, user_alice);

        EXPECT_LT(GetBalance(user_alice), sender_balance);
        EXPECT_EQ(
            balance_after_mine + (coin_to_send_ * i), GetBalance(user_bob));

        receiver_balance = GetBalance(user_bob);
        sender_balance = GetBalance(user_alice);

        ot::LogConsole()(
            "Bob balance after send " + GetDisplayBalance(receiver_balance))
            .Flush();
        ot::LogConsole()(
            "Alice balance after send " + GetDisplayBalance(sender_balance))
            .Flush();

        ot::LogConsole()("End of " + std::to_string(i) + " iteration").Flush();
    }

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

    CloseClient(user_bob.name_);
    CloseClient(user_alice.name_);

    // Restore both wallets
    const auto& user_alice_after_reboot = createSenderAlice();
    WaitForSynchro(user_alice_after_reboot, targetHeight, sender_balance);

    const auto& user_bob_after_reboot = createReceiverBob();
    WaitForSynchro(user_bob_after_reboot, targetHeight, receiver_balance);

    ot::LogConsole()(
        "Bob balance after reboot " + GetDisplayBalance(user_bob_after_reboot))
        .Flush();
    ot::LogConsole()(
        "Alice balance after reboot " +
        GetDisplayBalance(user_alice_after_reboot))
        .Flush();

    ot::LogConsole()(
        "Expected Bob balance after reboot " +
        GetDisplayBalance(receiver_balance))
        .Flush();
    ot::LogConsole()(
        "Expected Alice balance after reboot " +
        GetDisplayBalance(sender_balance))
        .Flush();

    // Compare balance
    EXPECT_EQ(GetBalance(user_bob_after_reboot), receiver_balance);
    EXPECT_EQ(GetBalance(user_alice_after_reboot), sender_balance);

    // Compare fee rates
    EXPECT_EQ(
        collectFeeRate(user_alice_after_reboot, user_bob_after_reboot),
        fee_rates);

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
}

TEST_F(Restart_fixture, shutdown) { Shutdown(); }

}  // namespace ottest
