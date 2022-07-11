// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/regtest/Single.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <utility>

namespace ottest
{
Regtest_fixture_single::Regtest_fixture_single(
    const ot::api::Context& ot,
    ot::Options clientArgs)
    : Regtest_fixture_normal(ot, 1, std::move(clientArgs))
{
}

Regtest_fixture_single::Regtest_fixture_single()
    : Regtest_fixture_normal(ot_, 1)
{
}
}  // namespace ottest
