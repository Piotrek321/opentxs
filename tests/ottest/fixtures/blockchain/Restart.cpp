// Copyright (c) 2010-2022 The Open-Transactions developers
// // This Source Code Form is subject to the terms of the Mozilla Public
// // License, v. 2.0. If a copy of the MPL was not distributed with this
// // file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/RegtestSimple.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <ostream>
#include <string_view>

#include "ottest/fixtures/blockchain/Restart.hpp"
#include "ottest/fixtures/common/User.hpp"

namespace ottest
{
const User& Restart_fixture::CreateUser(
    const std::string& name,
    const std::string& words)
{
    auto [user_alice, success_alice] =
        CreateClient(opentxs::Options{}, instance_++, name, words, address_);
    EXPECT_TRUE(success_alice);

    return user_alice;
}

}  // namespace ottest
