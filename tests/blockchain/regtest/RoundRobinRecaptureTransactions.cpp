// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "opentxs/util/Log.hpp"
#include "ottest/fixtures/blockchain/RegtestSimple.hpp"
#include "ottest/fixtures/paymentcode/VectorsV3.hpp"

namespace ottest
{

TEST_F(Regtest_fixture_round_robin, round_robin_recapture_transactions)
{
    const auto blocks_number = 2;
    auto coin_to_send =
        blocks_number * transaction_in_block_ * amount_in_transaction_ -
        1000000;

    auto& user_alice = users_.at(core_wallet_.name_);

    std::vector<std::reference_wrapper<const User>> users;

    for (auto& auxiliary_wallet : auxiliary_wallets_) {
        auto& user = users_.at(auxiliary_wallet.second.name_);
        users.push_back(user);
        user.expected_balance_ =
            blocks_number * transaction_in_block_ * amount_in_transaction_;
    }
    MineBlocksForUsers(users, target_height, blocks_number);

    // mine MaturationInterval number block with
    MineBlocks(target_height, static_cast<int>(MaturationInterval()) + 1);
    target_height += static_cast<int>(MaturationInterval()) + 1;

    for (auto& auxiliary_wallet : auxiliary_wallets_) {
        auto& user = users_.at(auxiliary_wallet.second.name_);
        SendCoins(user_alice, user, target_height, coin_to_send);
        user_alice.expected_balance_ += coin_to_send;

        auto& alice_balance = GetBalance(user_alice);
        auto& sender_balance = GetBalance(user);
        EXPECT_EQ(alice_balance, user_alice.expected_balance_);

        ot::LogConsole()(user_alice.name_)(" balance: ")(
            GetDisplayBalance(user_alice))
            .Flush();
        ot::LogConsole()(user.name_)(" balance: ")(
            GetDisplayBalance(sender_balance))
            .Flush();
    }

    auto expected_balance = user_alice.expected_balance_;

    CloseClient(core_wallet_.name_);

    auto [user_core, success_core] = CreateClient(
        opentxs::Options{},
        instance_++,
        core_wallet_.name_,
        core_wallet_.words_,
        address_);
    EXPECT_TRUE(success_core);

    WaitForSynchro(user_core, target_height, expected_balance);

    auto& alice_balance = GetBalance(user_core);

    ot::LogConsole()(user_core.name_)(" balance: ")(
        GetDisplayBalance(alice_balance))
        .Flush();

    std::this_thread::sleep_for(std::chrono::seconds(20));
    Shutdown();
}

}  // namespace ottest
