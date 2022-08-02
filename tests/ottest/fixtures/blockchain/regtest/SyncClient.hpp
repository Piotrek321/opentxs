// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <opentxs/opentxs.hpp>

#include "ottest/fixtures/blockchain/regtest/Normal.hpp"

namespace ottest
{
class Regtest_fixture_sync_client : public Regtest_fixture_normal
{
protected:
    Regtest_fixture_sync_client();
};
}  // namespace ottest
