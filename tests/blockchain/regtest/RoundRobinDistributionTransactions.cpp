// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/Regtest.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "opentxs/OT.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/blockchain/block/bitcoin/Block.hpp"
#include "opentxs/blockchain/block/bitcoin/Transaction.hpp"
#include "opentxs/blockchain/crypto/Account.hpp"
#include "opentxs/blockchain/crypto/AddressStyle.hpp"
#include "opentxs/blockchain/crypto/Element.hpp"
#include "opentxs/blockchain/crypto/HD.hpp"
#include "opentxs/blockchain/crypto/HDProtocol.hpp"
#include "opentxs/blockchain/crypto/Subaccount.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/util/Log.hpp"
#include "ottest/fixtures/blockchain/RegtestSimple.hpp"
#include "ottest/fixtures/paymentcode/VectorsV3.hpp"

namespace ottest
{

TEST_F(Regtest_fixture_simple, round_robin_distribution_transactions)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());

    const std::string name_alice = "Alice";
    const std::string name_bob = "Bob";
    const std::string name_chris = "Chris";
    auto chris_words =
        "eight upper indicate swift arch wrestle injury crystal super reward";

    Height begin = 0;
    const auto blocks_number = 2;

    struct UserDescription {
        ot::UnallocatedCString name_;
        ot::UnallocatedCString words_;
    };

    std::map<std::string, UserDescription> test_users;
    test_users.emplace(
        name_alice, UserDescription{name_alice, GetVectors3().alice_.words_});
    test_users.emplace(
        name_bob, UserDescription{name_bob, GetVectors3().bob_.words_});
    test_users.emplace(name_chris, UserDescription{name_chris, chris_words});

    auto instance = 2;
    for (auto& entry : test_users) {
        auto& user_to_create = entry.second;
        auto [user, success] = CreateClient(
            opentxs::Options{},
            instance++,
            user_to_create.name_,
            user_to_create.words_,
            address_);
        EXPECT_TRUE(success);
    }

    auto& user_alice = users_.at(name_alice);

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

        auto& sender_balance = GetBalance(*sender);
        auto& receiver_balance = GetBalance(*receiver);

        EXPECT_LT(sender_balance, sender->expected_balance_ - coin_to_send);
        EXPECT_EQ(receiver_balance, receiver->expected_balance_);

        sender->expected_balance_ = sender_balance;

        sender_it++;
        receiver_it++;
        count++;
    }

    ot::Sleep(std::chrono::seconds(20));
    Shutdown();
}

}  // namespace ottest
