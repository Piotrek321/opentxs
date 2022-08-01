// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <functional>
#include <string_view>
#include <tuple>
#include <utility>

#include "opentxs/util/Container.hpp"

namespace opentxs
{
enum class BlockchainProfile : std::uint8_t;
enum class ConnectionMode : std::int8_t;

/** A list of object IDs and their associated aliases
 *  * string: id of the stored object
 *  * string: alias of the stored object
 */
using ObjectList =
    UnallocatedList<std::pair<UnallocatedCString, UnallocatedCString>>;
using SimpleCallback = std::function<void()>;

auto print(BlockchainProfile) noexcept -> std::string_view;
}  // namespace opentxs
