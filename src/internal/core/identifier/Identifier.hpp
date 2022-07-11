// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/core/identifier/Algorithm.hpp"
// IWYU pragma: no_include "opentxs/core/identifier/Type.hpp"

#pragma once

#include <string_view>

#include "opentxs/core/identifier/Types.hpp"

namespace opentxs
{
using namespace std::literals;

static constexpr auto identifier_header_ =
    sizeof(identifier::Algorithm) + sizeof(identifier::Type);
static constexpr auto identifier_prefix_ = "ot"sv;
}  // namespace opentxs
