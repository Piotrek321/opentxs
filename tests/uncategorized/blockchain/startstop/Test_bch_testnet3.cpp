// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <memory>

#include "ottest/fixtures/blockchain/StartStop.hpp"

namespace ottest
{
TEST_F(Test_StartStop, init_opentxs) {}

TEST_F(Test_StartStop, bch_testnet3)
{
    EXPECT_TRUE(api_.Network().Blockchain().Start(
        b::Type::BitcoinCash_testnet3, "127.0.0.2"));
    EXPECT_TRUE(
        api_.Network().Blockchain().Stop(b::Type::BitcoinCash_testnet3));
}
}  // namespace ottest
