// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.ko

#include <gtest/gtest.h>
#include <thread>

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

    TEST_F(Restart_fixture, mine_restore_do_not_wait_for_synchro)
    {
        ot::Amount balance_after_mine = amount_in_transaction_ * blocks_number_ * transaction_in_block_;

        // Create wallets
        const auto& user_bob = createReceiverBob();

        // Get data for later validation
        const auto bobs_payment_code = user_bob.PaymentCode();
        const auto bobs_hd_name = GetHDAccount(user_bob).Name();
        auto bob_address = GetWalletAddress(user_bob);
        EXPECT_EQ(GetWalletName(user_bob), name_bob);
        EXPECT_EQ(0, GetBalance(user_bob));

        // Mine initial balance
        {
            targetHeight += static_cast<Height>(blocks_number_);
            auto scan_listener = std::make_unique<ScanListener>(*user_bob.api_);

            auto scan_listener_external_f = scan_listener->get_future(
                    GetHDAccount(user_bob), bca::Subchain::External, targetHeight);
            auto scan_listener_internal_f = scan_listener->get_future(
                    GetHDAccount(user_bob), bca::Subchain::Internal, targetHeight);

            Height begin = 0;

            // mine coin for Alice
            auto mined_header = MineBlocks(
                    user_bob,
                    begin,
                    blocks_number_,
                    transaction_in_block_,
                    amount_in_transaction_);

            EXPECT_TRUE(scan_listener->wait(scan_listener_external_f));
            EXPECT_TRUE(scan_listener->wait(scan_listener_internal_f));

            begin += blocks_number_;
            auto count = static_cast<int>(MaturationInterval());
            targetHeight += count;

            scan_listener_external_f = scan_listener->get_future(
                    GetHDAccount(user_bob), bca::Subchain::External, targetHeight);
            scan_listener_internal_f = scan_listener->get_future(
                    GetHDAccount(user_bob), bca::Subchain::Internal, targetHeight);

            // mine MaturationInterval number block with
            MineBlocks(begin, count);

            EXPECT_TRUE(scan_listener->wait(scan_listener_external_f));
            EXPECT_TRUE(scan_listener->wait(scan_listener_internal_f));
            WaitForSynchro(user_bob, targetHeight, balance_after_mine);
        }
        EXPECT_EQ(balance_after_mine, GetBalance(user_bob));

        // Cleanup
        CloseClient(user_bob.name_);

        const auto& user_bob_after_reboot = createReceiverBob();

        // Compare balance and transactions
        EXPECT_NE(balance_after_mine, GetBalance(user_bob_after_reboot));
        EXPECT_EQ(0, GetTransactions(user_bob_after_reboot).size());

        // Compare names and addresses
        EXPECT_EQ(name_bob, GetWalletName(user_bob_after_reboot));
        EXPECT_EQ(bob_address, GetWalletAddress(user_bob_after_reboot));
        EXPECT_EQ(bobs_hd_name, GetHDAccount(user_bob_after_reboot).Name());
        EXPECT_EQ(bobs_payment_code, user_bob_after_reboot.PaymentCode());

        EXPECT_EQ(targetHeight, GetHeight(user_bob_after_reboot));

        CloseClient(user_bob_after_reboot.name_);
    }

    TEST_F(Restart_fixture, end) { Shutdown(); }

}  // namespace ottest
