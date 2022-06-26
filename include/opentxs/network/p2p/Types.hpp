// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/network/p2p/Block.hpp"
// IWYU pragma: no_include "opentxs/network/p2p/State.hpp"

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <string_view>

#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace network
{
namespace p2p
{
class Block;
class State;
}  // namespace p2p
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::network::p2p
{
using TypeEnum = std::uint32_t;

enum class MessageType : TypeEnum;

using StateData = Vector<p2p::State>;
using SyncData = Vector<p2p::Block>;

OPENTXS_EXPORT auto print(MessageType in) noexcept -> std::string_view;

constexpr auto value(MessageType type) noexcept
{
    return static_cast<TypeEnum>(type);
}
}  // namespace opentxs::network::p2p
