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
#include "ottest/fixtures/ui/ContactList.hpp"

namespace ottest
{
const int number_of_tests = 10;

TEST_F(Restart_fixture, send_multiple_transactions_remove_user_compare)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());
    ot::LogConsole()("send_multiple_transactions_remove_user_compare start")
        .Flush();
    ot::Amount receiver_balance, sender_balance;

    // Create wallets
    const auto& user_alice = CreateUser(name_alice_, words_alice_);
    const auto& user_bob = CreateUser(name_bob_, words_bob_);

    std::vector<std::reference_wrapper<const User>> users{user_bob, user_alice};
    // Mine initial balance
    MineBlocksForUsers(users, current_height_, blocks_number_);

    receiver_balance = GetBalance(user_bob);
    sender_balance = GetBalance(user_alice);

    for (int i = 0; i < number_of_tests; ++i) {
        ot::LogConsole()("iteration no: " + std::to_string(i + 1)).Flush();

        // Send coins from alice to bob
        SendCoins(user_bob, user_alice, current_height_);

        EXPECT_LT(GetBalance(user_alice), sender_balance);
        EXPECT_EQ(
            balance_after_mine_ + (coin_to_send_ * (i + 1)),
            GetBalance(user_bob));

        receiver_balance = GetBalance(user_bob);
        sender_balance = GetBalance(user_alice);

        ot::LogConsole()(
            "Bob balance after send " + GetDisplayBalance(receiver_balance))
            .Flush();
        ot::LogConsole()(
            "Alice balance after send " + GetDisplayBalance(sender_balance))
            .Flush();

        ot::LogConsole()("End of " + std::to_string(i + 1) + " iteration")
            .Flush();
    }

    // Collect outputs
    std::set<UTXO> bob_outputs;
    std::set<UTXO> alice_outputs;
    std::map<ot::blockchain::node::TxoState, std::size_t>
        bob_number_of_outputs_per_type;
    std::map<ot::blockchain::node::TxoState, std::size_t>
        alice_number_of_outputs_per_type;

    CollectOutputs(user_bob, bob_outputs, bob_number_of_outputs_per_type);

    CollectOutputs(user_alice, alice_outputs, alice_number_of_outputs_per_type);

    CloseClient(user_bob.name_);
    CloseClient(user_alice.name_);

    // Restore both wallets
    const auto& user_alice_after_reboot = CreateUser(name_alice_, words_alice_);
    WaitForSynchro(user_alice_after_reboot, current_height_, sender_balance);

    const auto& user_bob_after_reboot = CreateUser(name_bob_, words_bob_);
    WaitForSynchro(user_bob_after_reboot, current_height_, receiver_balance);

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

    // TODO verify the logical correctness after the following cal was changed
    // to refer to Alice 2 rather than the deleted Alice
    auto loaded_transactions = CollectTransactionsForFeeCalculations(
        user_alice_after_reboot, send_transactions_, transactions_ptxid_);
    auto fee = CalculateFee(send_transactions_, loaded_transactions);

    EXPECT_EQ(
        Amount{balance_after_mine_} - Amount{coin_to_send_ * number_of_tests} -
            fee,
        GetBalance(user_alice_after_reboot));

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
    Shutdown();
}

}  // namespace ottest
