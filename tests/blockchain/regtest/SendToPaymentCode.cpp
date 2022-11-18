// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "opentxs/OT.hpp"
#include "ottest/data/crypto/PaymentCodeV3.hpp"
#include "ottest/fixtures/blockchain/Regtest.hpp"
#include "ottest/fixtures/blockchain/RegtestSimple.hpp"

namespace ottest
{

TEST_F(Regtest_fixture_simple, send_to_payment_code)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());

    const std::string name_alice = "Alice";
    const std::string name_bob = "Bob";

    Height begin = 0;
    const auto blocks_number = 2;
    auto coin_to_send = 100000;
    const auto expected_balance = amount_in_transaction_ * blocks_number * transaction_in_block_;

    auto [user_alice, success_alice] = CreateClient(
        opentxs::Options{},
        3,
        core_wallet_.name_,
        core_wallet_.words_,
        address_);
    EXPECT_TRUE(success_alice);

    auto [user_bob, success_bob] = CreateClient(
        opentxs::Options{},
        4,
        name_bob,
        GetPaymentCodeVector3().bob_.words_,
        address_);
    EXPECT_TRUE(success_bob);

    std::vector<std::reference_wrapper<const User>> users{user_alice, user_bob};
    MineBlocksForUsers(users, target_height, blocks_number);
    begin = target_height;
    target_height += static_cast<int>(MaturationInterval()) + 1;

    // mine MaturationInterval number block with
    MineBlocks(begin, static_cast<int>(MaturationInterval()) + 1);
    begin += MaturationInterval() + 1;

    EXPECT_EQ(
        GetBalance(user_alice),
        expected_balance);

    EXPECT_EQ(
        GetBalance(user_bob),
        expected_balance);

    User* sender = &users_.at(name_alice);
    User* receiver = &users_.at(name_bob);
    sender->expected_balance_ =
        expected_balance;
    receiver->expected_balance_ =
        expected_balance;

    const size_t numbers_of_test = 2;

    for (size_t number_of_test = 0; number_of_test < numbers_of_test;
         number_of_test++) {

        const auto& network =
            sender->api_->Network().Blockchain().GetChain(test_chain_);

        auto future = network.SendToPaymentCode(
            sender->nym_id_,
            receiver->PaymentCode(),
            coin_to_send,
            "memo for outgoing transaction");

        MineTransaction(*sender, future.get().second, target_height);

        auto loaded_transactions = CollectTransactionsForFeeCalculations(
            *sender, send_transactions_, transactions_ptxid_);
        auto fee = CalculateFee(send_transactions_, loaded_transactions);
        send_transactions_.clear();

        sender->expected_balance_ -= Amount{coin_to_send} + fee;
        receiver->expected_balance_ += coin_to_send;

        WaitForSynchro(*sender, target_height, sender->expected_balance_);
        WaitForSynchro(*receiver, target_height, receiver->expected_balance_);

        EXPECT_EQ(GetBalance(*sender), sender->expected_balance_);
        EXPECT_EQ(GetBalance(*receiver), receiver->expected_balance_);

        std::swap(sender, receiver);
    }

    Shutdown();
}
}  // namespace ottest
