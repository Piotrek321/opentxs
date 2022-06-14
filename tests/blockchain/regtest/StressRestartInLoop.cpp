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

TEST_F(Restart_fixture, send_remove_user_compare_repeat)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());
    ot::LogConsole()("send_remove_user_compare_repeat start").Flush();
    // Create wallets
    ot::Amount receiver_balance{};
    ot::Amount sender_balance{};

    for (int i = 0; i < number_of_tests; ++i) {
        ot::LogConsole()("iteration no: " + std::to_string(i + 1)).Flush();

        const auto& user_alice = CreateUser(name_alice_, words_alice_);
        const auto& user_bob = CreateUser(name_bob_, words_bob_);

        if (!i) {
            std::vector<std::reference_wrapper<const User>> users{
                user_bob, user_alice};
            // Mine initial balance
            MineBlocksForUsers(users, current_height_, blocks_number_);

            std::cerr << "QQQ about to WaitForSynchro 1a, i=" << i << "\n";
            WaitForSynchro(
                user_alice,
                current_height_,
                amount_in_transaction_ * blocks_number_ *
                    transaction_in_block_);
            std::cerr << "QQQ about to WaitForSynchro 2a, i=" << i << "\n";
            WaitForSynchro(
                user_bob,
                current_height_,
                amount_in_transaction_ * blocks_number_ *
                    transaction_in_block_);
        } else {
            std::cerr << "QQQ about to WaitForSynchro 1, i=" << i << "\n";
            WaitForSynchro(user_alice, current_height_, sender_balance);
            std::cerr << "QQQ about to WaitForSynchro 2, i=" << i << "\n";
            WaitForSynchro(user_bob, current_height_, receiver_balance);
        }
        sender_balance = GetBalance(user_alice);

        // Send coins from alice to bob
        SendCoins(user_bob, user_alice, current_height_);

        std::cerr << "QQQ about to WaitForSynchro 3, i=" << i << "\n";
        WaitForSynchro(
            user_bob,
            current_height_,
            balance_after_mine_ + (coin_to_send_ * (i + 1)));

        receiver_balance = GetBalance(user_bob);

        auto loaded_transactions = CollectTransactionsForFeeCalculations(
            user_alice, send_transactions_, transactions_ptxid_);
        auto fee = CalculateFee(send_transactions_, loaded_transactions);

        std::cerr << "QQQ about to WaitForSynchro 4, i=" << i << "\n";
        WaitForSynchro(
            user_alice,
            current_height_,
            Amount{balance_after_mine_} - Amount{coin_to_send_ * (i + 1)} -
                fee);

        std::cerr << "QQQ returned from WaitForSynchro 4, i=" << i << "\n";
        EXPECT_EQ(
            Amount{balance_after_mine_} - Amount{coin_to_send_ * (i + 1)} - fee,
            GetBalance(user_alice));

        sender_balance = GetBalance(user_alice);

        EXPECT_EQ(
            balance_after_mine_ + (coin_to_send_ * (i + 1)), receiver_balance);

        ot::LogConsole()(
            "Bob balance after send " + GetDisplayBalance(receiver_balance))
            .Flush();
        ot::LogConsole()(
            "Alice balance after send " + GetDisplayBalance(sender_balance))
            .Flush();

        // Collect outputs
        std::set<UTXO> bob_outputs;
        std::set<UTXO> alice_outputs;
        std::map<ot::blockchain::node::TxoState, std::size_t>
            bob_number_of_outputs_per_type;
        std::map<ot::blockchain::node::TxoState, std::size_t>
            alice_number_of_outputs_per_type;

        CollectOutputs(user_bob, bob_outputs, bob_number_of_outputs_per_type);
        CollectOutputs(
            user_alice, alice_outputs, alice_number_of_outputs_per_type);

        CloseClient(user_bob.name_);
        CloseClient(user_alice.name_);

        // Restore both wallets
        const auto& user_alice_after_reboot =
            CreateUser(name_alice_, words_alice_);

        std::cerr << "QQQ about to WaitForSynchro 5, i=" << i << "\n";
        WaitForSynchro(
            user_alice_after_reboot, current_height_, sender_balance);

        const auto& user_bob_after_reboot = CreateUser(name_bob_, words_bob_);
        std::cerr << "QQQ about to WaitForSynchro 6, i=" << i << "\n";
        WaitForSynchro(
            user_bob_after_reboot, current_height_, receiver_balance);

        ot::LogConsole()(
            "Bob balance after reboot " +
            GetDisplayBalance(user_bob_after_reboot))
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
            "Expected  Alice balance after reboot " +
            GetDisplayBalance(sender_balance))
            .Flush();

        // Compare balance
        EXPECT_EQ(GetBalance(user_bob_after_reboot), receiver_balance);
        EXPECT_EQ(GetBalance(user_alice_after_reboot), sender_balance);

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
        ot::LogConsole()("End of " + std::to_string(i + 1) + " iteration")
            .Flush();
    }

    ot::LogConsole()("send_remove_user_compare_repeat end").Flush();

    Shutdown();
}

}  // namespace ottest
