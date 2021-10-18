// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <mutex>
#include <optional>

#include "opentxs/Bytes.hpp"
#include "opentxs/blockchain/Blockchain.hpp"

namespace opentxs
{
namespace api
{
class Core;
}  // namespace api
}  // namespace opentxs

namespace opentxs::blockchain::database::wallet::db
{
struct Position {
    const Space data_;

    auto Decode(const api::Core& api) const noexcept -> const block::Position&;
    auto Hash() const noexcept -> ReadView;
    auto Height() const noexcept -> block::Height;

    Position(const block::Position& position) noexcept;
    Position(const ReadView bytes) noexcept(false);

    ~Position() = default;

private:
    static constexpr auto fixed_ = std::size_t{sizeof(block::Height) + 32u};

    mutable std::mutex lock_;
    mutable std::optional<block::Position> position_;

    Position() = delete;
    Position(const Position&) = delete;
    Position(Position&&) = delete;
    auto operator=(const Position&) -> Position& = delete;
    auto operator=(Position&&) -> Position& = delete;
};
}  // namespace opentxs::blockchain::database::wallet::db
