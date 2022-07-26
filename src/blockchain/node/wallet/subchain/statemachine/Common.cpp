// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <boost/smart_ptr/detail/operator_bool.hpp>

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "internal/blockchain/node/wallet/subchain/statemachine/Types.hpp"  // IWYU pragma: associated

#include <cstring>
#include <iterator>

#include "internal/util/LogMacros.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameIterator.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"

namespace opentxs::blockchain::node::wallet
{
auto decode(
    const api::Session& api,
    network::zeromq::Message& in,
    Set<ScanStatus>& clean,
    Set<block::Position>& dirty) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(2 < body.size());

    for (auto f = std::next(body.begin(), 2), end = body.end(); f != end; ++f) {
        const auto bytes = f->size();
        static constexpr auto fixed =
            std::size_t{sizeof(ScanState) + sizeof(block::Height)};
        static_assert(9 == fixed);

        OT_ASSERT(fixed < bytes);
        // NOTE this assert assumes 32 byte hash, which might not be true
        // someday but is true in all cases now.
        OT_ASSERT(41 == bytes);

        const auto* i = static_cast<const std::byte*>(f->data());
        const auto type =
            static_cast<ScanState>(*reinterpret_cast<const std::uint8_t*>(i));
        std::advance(i, sizeof(ScanState));

        block::Height height{};
        std::memcpy(&height, i, sizeof(height));
        std::advance(i, sizeof(height));

        block::Hash hash{};
        const auto rc = hash.Assign(i, bytes - fixed);
        OT_ASSERT(rc);

        if (ScanState::dirty == type) {
            dirty.emplace(std::make_pair(height, std::move(hash)));
        } else {
            clean.emplace(type, std::make_pair(height, std::move(hash)));
        }
    }
}

auto encode(
    const Vector<ScanStatus>& in,
    network::zeromq::Message& out) noexcept -> void
{
    for (const auto& status : in) { encode(status, out); }
}

auto encode(const ScanStatus& in, network::zeromq::Message& out) noexcept
    -> void
{
    static constexpr auto fixed = sizeof(ScanState) + sizeof(block::Height);
    const auto& [status, position] = in;
    const auto& [height, hash] = position;
    const auto size = fixed + hash.size();  // TODO constexpr
    auto bytes = out.AppendBytes()(size);

    OT_ASSERT(bytes.valid(size));

    auto* i = bytes.as<std::byte>();
    std::memcpy(i, &status, sizeof(status));
    std::advance(i, sizeof(status));
    std::memcpy(i, &height, sizeof(height));
    std::advance(i, sizeof(height));
    std::memcpy(i, hash.data(), hash.size());
}

auto extract_dirty(
    const api::Session& api,
    network::zeromq::Message& in,
    Vector<ScanStatus>& out) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(2 < body.size());

    for (auto f = std::next(body.begin(), 2), end = body.end(); f != end; ++f) {
        const auto bytes = f->size();
        static constexpr auto fixed =
            std::size_t{sizeof(ScanState) + sizeof(block::Height)};
        static_assert(9 == fixed);

        OT_ASSERT(fixed < f->size());
        // NOTE this assert assumes 32 byte hash, which might not be true
        // someday but is true in all cases now.
        OT_ASSERT(41 == bytes);

        const auto* i = static_cast<const std::byte*>(f->data());
        const auto type =
            static_cast<ScanState>(*reinterpret_cast<const std::uint8_t*>(i));
        std::advance(i, sizeof(ScanState));

        if (ScanState::dirty != type) { continue; }

        block::Height height{};
        std::memcpy(&height, i, sizeof(height));
        std::advance(i, sizeof(height));

        block::Hash hash{};
        const auto rc = hash.Assign(i, bytes - fixed);
        OT_ASSERT(rc);

        out.emplace_back(type, std::make_pair(height, std::move(hash)));
    }
}
}  // namespace opentxs::blockchain::node::wallet
