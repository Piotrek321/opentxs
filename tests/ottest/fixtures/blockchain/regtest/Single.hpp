// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <opentxs/opentxs.hpp>

#include "ottest/fixtures/blockchain/regtest/Normal.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
// inline namespace v1
// {
namespace opentxs
{
class Options;
}  // namespace opentxs
// }  // namespace v1
// NOLINTEND(modernize-concat-nested-namespaces)

namespace ottest
{
class Regtest_fixture_single : public Regtest_fixture_normal
{
protected:
    Regtest_fixture_single();
    Regtest_fixture_single(ot::Options clientArgs);
};
}  // namespace ottest
