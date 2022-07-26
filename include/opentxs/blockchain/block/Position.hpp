// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstddef>
#include <functional>
#include <string_view>
#include <tuple>
#include <utility>

#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
namespace blockchain
{
namespace block
{
class Position;
}  // namespace block
}  // namespace blockchain
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace std
{
template <>
struct OPENTXS_EXPORT hash<opentxs::blockchain::block::Position> {
    auto operator()(const opentxs::blockchain::block::Position& data)
        const noexcept -> std::size_t;
};

template <>
struct OPENTXS_EXPORT less<opentxs::blockchain::block::Position> {
    auto operator()(
        const opentxs::blockchain::block::Position& lhs,
        const opentxs::blockchain::block::Position& rhs) const noexcept -> bool;
};
}  // namespace std

namespace opentxs::blockchain::block
{
auto swap(Position& lhs, Position& rhs) noexcept -> void;
class OPENTXS_EXPORT Position
{
public:
    Height height_;
    Hash hash_;

    auto operator==(const Position& rhs) const noexcept -> bool;
    auto operator!=(const Position& rhs) const noexcept -> bool;
    auto operator<(const Position& rhs) const noexcept -> bool;
    auto operator<=(const Position& rhs) const noexcept -> bool;
    /// Use this to test for a chain reorg. rhs is the new position which may or
    /// may not replace the current position.
    auto operator>(const Position& rhs) const noexcept -> bool;
    auto operator>=(const Position& rhs) const noexcept -> bool;
    auto print() const noexcept -> UnallocatedCString;
    auto print(alloc::Default alloc) const noexcept -> CString;

    auto swap(Position& rhs) noexcept -> void;

    Position() noexcept;
    Position(const Height& height, const Hash& hash) noexcept;
    Position(const Height& height, Hash&& hash) noexcept;
    Position(const Height& height, ReadView hash) noexcept;
    Position(Height&& height, const Hash& hash) noexcept;
    Position(Height&& height, Hash&& hash) noexcept;
    Position(Height&& height, ReadView hash) noexcept;
    // TODO: MT-110
    Position(const std::pair<Height, Hash>& data) noexcept;
    Position(std::pair<Height, Hash>&& data) noexcept;
    Position(const Position& rhs) noexcept;
    Position(Position&& rhs) noexcept;
    auto operator=(const Position& rhs) noexcept -> Position&;
    auto operator=(Position&& rhs) noexcept -> Position&;
};
}  // namespace opentxs::blockchain::block
