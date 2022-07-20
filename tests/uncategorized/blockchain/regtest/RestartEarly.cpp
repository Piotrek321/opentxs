// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.ko

#include "ottest/fixtures/common/User.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <thread>

#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/block/Outpoint.hpp"
#include "ottest/fixtures/blockchain/Restart.hpp"

namespace ottest
{

TEST_F(Restart_fixture, mine_restore_do_not_wait_for_synchro)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());
    ot::Amount balance_after_mine =
        amount_in_transaction_ * blocks_number_ * transaction_in_block_;

    // Create wallets
    const auto& user_bob = CreateUser(name_bob_, words_bob_);

    // Get data for later validation
    const auto bobs_payment_code = user_bob.PaymentCode();
    const auto bobs_hd_name = GetHDAccount(user_bob).Name();
    auto bob_address = GetWalletAddress(user_bob);
    EXPECT_EQ(GetWalletName(user_bob), name_bob_);
    EXPECT_EQ(0, GetBalance(user_bob));

    std::vector<std::reference_wrapper<const User>> users{user_bob};
    // Mine initial balance
    MineBlocksForUsers(users, current_height_, blocks_number_);

    EXPECT_EQ(balance_after_mine, GetBalance(user_bob));

    // Cleanup
    CloseClient(user_bob.name_);

    const auto& user_bob_after_reboot = CreateUser(name_bob_, words_bob_);

    // Compare balance and transactions
    EXPECT_NE(balance_after_mine, GetBalance(user_bob_after_reboot));
    EXPECT_EQ(0, GetTransactions(user_bob_after_reboot).size());

    // Compare names and addresses
    EXPECT_EQ(name_bob_, GetWalletName(user_bob_after_reboot));
    EXPECT_EQ(bob_address, GetWalletAddress(user_bob_after_reboot));
    EXPECT_EQ(bobs_hd_name, GetHDAccount(user_bob_after_reboot).Name());
    EXPECT_EQ(bobs_payment_code, user_bob_after_reboot.PaymentCode());

    EXPECT_EQ(current_height_, GetHeight(user_bob_after_reboot));

    CloseClient(user_bob_after_reboot.name_);
    Shutdown();
}

}  // namespace ottest
