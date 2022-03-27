// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

#include "internal/blockchain/node/Node.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/crypto/Types.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace blockchain
{
namespace block
{
class Outpoint;
}  // namespace block
}  // namespace blockchain

class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::wallet
{
class ElementCache final : public Allocated
{
public:
    using Map = internal::WalletDatabase::ElementMap;
    using Patterns = internal::WalletDatabase::Patterns;
    using TXOs = internal::WalletDatabase::TXOs;

    struct Matches final : public Allocated {
        Set<Bip32Index> match_20_;
        Set<Bip32Index> match_32_;
        Set<Bip32Index> match_33_;
        Set<Bip32Index> match_64_;
        Set<Bip32Index> match_65_;
        Set<block::Outpoint> match_txo_;

        auto get_allocator() const noexcept -> allocator_type final;

        auto Merge(Matches&& rhs) noexcept -> void;

        Matches(allocator_type alloc = {}) noexcept;
        Matches(const Matches& rhs, allocator_type alloc = {}) noexcept;
        Matches(Matches&& rhs) noexcept;
        Matches(Matches&& rhs, allocator_type alloc) noexcept;
        auto operator=(const Matches& rhs) noexcept -> Matches&;
        auto operator=(Matches&& rhs) noexcept -> Matches&;

        ~Matches() final = default;
    };

    struct Index final : public Allocated {
        Matches confirmed_no_match_;
        Matches confirmed_match_;

        auto get_allocator() const noexcept -> allocator_type final;

        auto Merge(Index&& rhs) noexcept -> void;

        Index(allocator_type alloc = {}) noexcept;
        Index(const Index& rhs, allocator_type alloc = {}) noexcept;
        Index(Index&& rhs) noexcept;
        Index(Index&& rhs, allocator_type alloc) noexcept;
        auto operator=(const Index& rhs) noexcept -> Index&;
        auto operator=(Index&& rhs) noexcept -> Index&;

        ~Index() final = default;
    };
    using Results = opentxs::Map<block::Position, Index>;

    struct Elements final : public Allocated {
        Vector<std::pair<Bip32Index, std::array<std::byte, 20>>> elements_20_;
        Vector<std::pair<Bip32Index, std::array<std::byte, 32>>> elements_32_;
        Vector<std::pair<Bip32Index, std::array<std::byte, 33>>> elements_33_;
        Vector<std::pair<Bip32Index, std::array<std::byte, 64>>> elements_64_;
        Vector<std::pair<Bip32Index, std::array<std::byte, 65>>> elements_65_;
        TXOs txos_;
        Results results_;

        auto get_allocator() const noexcept -> allocator_type final;
        auto size() const noexcept -> std::size_t;

        Elements(allocator_type alloc = {}) noexcept;
        Elements(const Elements& rhs, allocator_type alloc = {}) noexcept;
        Elements(Elements&& rhs) noexcept;
        Elements(Elements&& rhs, allocator_type alloc) noexcept;
        auto operator=(const Elements& rhs) noexcept -> Elements&;
        auto operator=(Elements&& rhs) noexcept -> Elements&;

        ~Elements() final = default;
    };

    auto Get(allocator_type alloc = {}) const noexcept -> Elements;
    auto get_allocator() const noexcept -> allocator_type final;

    auto Add(Map&& data) noexcept -> void;
    auto Add(TXOs&& created, TXOs&& consumed) noexcept -> void;
    auto Add(Results&& results) noexcept -> void;
    auto Forget(const block::Position& last) noexcept -> void;

    ElementCache(
        Patterns&& data,
        Vector<internal::WalletDatabase::UTXO>&& txos,
        allocator_type alloc) noexcept;

    ~ElementCache() final;

private:
    const Log& log_;
    Map data_;
    Elements cache_;

    static auto convert(Patterns&& in, allocator_type alloc = {}) noexcept
        -> Map;

    auto index(const Map::value_type& data) noexcept -> void;
    auto index(
        const Bip32Index index,
        const Vector<std::byte>& element) noexcept -> void;
};
}  // namespace opentxs::blockchain::node::wallet
