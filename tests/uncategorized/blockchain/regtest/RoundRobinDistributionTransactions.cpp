// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/common/User.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "opentxs/OT.hpp"
#include "opentxs/api/Context.hpp"
#include "ottest/data/crypto/PaymentCodeV3.hpp"
#include "ottest/fixtures/blockchain/Regtest.hpp"
#include "ottest/fixtures/blockchain/RegtestSimple.hpp"


namespace ottest
{

TEST_F(Regtest_fixture_round_robin, round_robin_distribution_transactions)
{
    Height begin = 0;
    const auto blocks_number = 2;

    auto& user_alice = users_.at(core_wallet_.name_);

    target_height += blocks_number;

    // mine coin for Alice
    auto mined_header = MineBlocks(
        user_alice,
        begin,
        blocks_number,
        transaction_in_block_,
        amount_in_transaction_);

    begin += blocks_number;
    user_alice.expected_balance_ =
        blocks_number * transaction_in_block_ * amount_in_transaction_;
    target_height += static_cast<int>(MaturationInterval()) + 1;

    // mine MaturationInterval number block
    MineBlocks(begin, static_cast<int>(MaturationInterval()) + 1);
    begin += MaturationInterval() + 1;

    auto sender_it = users_.begin();
    auto receiver_it = std::next(sender_it);
    auto count = 1;
    auto alice_balance =
        blocks_number * transaction_in_block_ * amount_in_transaction_;
    while (sender_it != users_.end()) {
        User* sender = &sender_it->second;
        User* receiver = receiver_it != users_.end() ? &receiver_it->second
                                                     : &users_.begin()->second;

        auto coin_to_send = alice_balance - count * 100000000;
        SendCoins(*receiver, *sender, target_height, coin_to_send);

        receiver->expected_balance_ += coin_to_send;
        WaitForSynchro(*receiver, target_height, receiver->expected_balance_);

        auto loaded_transactions = CollectTransactionsForFeeCalculations(
            *sender, send_transactions_, transactions_ptxid_);
        auto fee = CalculateFee(send_transactions_, loaded_transactions);
        send_transactions_.clear();

        sender->expected_balance_ -= Amount{coin_to_send} + fee;

        auto& sender_balance = GetBalance(*sender);
        auto& receiver_balance = GetBalance(*receiver);

        EXPECT_EQ(sender_balance, sender->expected_balance_);
        EXPECT_EQ(receiver_balance, receiver->expected_balance_);

        sender_it++;
        receiver_it++;
        count++;
    }

    Shutdown();
}

}  // namespace ottest
