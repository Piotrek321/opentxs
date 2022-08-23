// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.ko

#include "ottest/fixtures/blockchain/Regtest.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <iostream>
#include <thread>

#include "internal/identity/Nym.hpp"
#include "ottest/data/crypto/PaymentCodeV3.hpp"
#include "ottest/fixtures/blockchain/RegtestSimple.hpp"
#include "ottest/fixtures/ui/AccountTree.hpp"

namespace ottest
{
const std::string name_andrew = "Andrew";
const std::string name_bob_ = "Bob";

TEST_F(
    Regtest_fixture_simple,
    create_wallet_with_some_name_reboot_with_different_name_and_same_seed_compare)
{
    EXPECT_TRUE(Start());
    EXPECT_TRUE(Connect());
    // Create wallets
    auto [user_bob, success] = CreateClient(
        opentxs::Options{},
        3,
        name_bob_,
        GetPaymentCodeVector3().bob_.words_,
        address_);
    EXPECT_TRUE(success);

    auto bob_address = GetWalletAddress(user_bob);

    EXPECT_EQ(GetWalletName(user_bob), name_bob_);

    auto& nym_no_const = const_cast<opentxs::identity::internal::Nym&>(
        user_bob.nym_->Internal());
    nym_no_const.SetAlias(name_andrew);

    EXPECT_EQ(GetWalletName(user_bob), name_andrew);

    // Cleanup
    CloseClient(user_bob.name_);
    ot::LogConsole()("User removed");

    // Create user with same seed but different name
    auto [user_andrew_after_reboot, success2] = CreateClient(
        opentxs::Options{},
        3,
        name_andrew,
        GetPaymentCodeVector3().bob_.words_,
        address_);

    EXPECT_TRUE(success2);

    // Compare name and address
    EXPECT_EQ(GetWalletName(user_andrew_after_reboot), name_andrew);
    EXPECT_EQ(bob_address, GetWalletAddress(user_andrew_after_reboot));

    CloseClient(user_andrew_after_reboot.name_);
    Shutdown();
}

}  // namespace ottest
