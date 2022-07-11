// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>

#include "ottest/Basic.hpp"

namespace ottest
{
ot::UnallocatedCString server_id_{};

TEST(ServerSession, create)
{
    const auto& otx = ot::InitContext(Args(true));
    const auto& server = otx.StartNotarySession(0);
    server_id_ = server.ID().asBase58(otx.Crypto());

    EXPECT_FALSE(server_id_.empty());

    ot::Cleanup();
}

TEST(ServerSession, restart)
{
    const auto& otx = ot::InitContext(Args(true));
    const auto& server = otx.StartNotarySession(0);

    EXPECT_EQ(server_id_, server.ID().asBase58(otx.Crypto()));

    ot::Cleanup();
}
}  // namespace ottest
