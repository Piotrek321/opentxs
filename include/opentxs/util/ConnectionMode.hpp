// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include "opentxs/util/Types.hpp"

namespace opentxs
{
enum class ConnectionMode : std::int8_t {
    off = -1,
    automatic = 0,
    on = 1,
};
}  // namespace opentxs
