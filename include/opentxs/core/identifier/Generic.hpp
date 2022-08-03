// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/core/identifier/Algorithm.hpp"
// IWYU pragma: no_include "opentxs/core/identifier/Type.hpp"

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

#include "opentxs/core/Data.hpp"
#include "opentxs/core/identifier/Types.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
class Crypto;
}  // namespace api

namespace proto
{
class Identifier;
}  // namespace proto

namespace identifier
{
class Generic;
}  // namespace identifier

class String;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace std
{
template <>
struct OPENTXS_EXPORT hash<opentxs::identifier::Generic> {
    auto operator()(const opentxs::identifier::Generic& data) const noexcept
        -> std::size_t;
};

template <>
struct OPENTXS_EXPORT less<opentxs::identifier::Generic> {
    auto operator()(
        const opentxs::identifier::Generic& lhs,
        const opentxs::identifier::Generic& rhs) const -> bool;
};
}  // namespace std

namespace opentxs
{
OPENTXS_EXPORT auto default_identifier_algorithm() noexcept
    -> identifier::Algorithm;
OPENTXS_EXPORT auto identifier_expected_hash_bytes(
    identifier::Algorithm type) noexcept -> std::size_t;
}  // namespace opentxs

namespace opentxs::identifier
{
/** An Identifier is basically a 256 bit hash value. This class makes it easy to
 * convert IDs back and forth to strings. */
class OPENTXS_EXPORT Generic : virtual public Allocated, virtual public Data
{
public:
    class Imp;

    auto Algorithm() const noexcept -> identifier::Algorithm;
    auto asBase58(const api::Crypto& api) const -> UnallocatedCString;
    auto asBase58(const api::Crypto& api, alloc::Default alloc) const
        -> CString;
    auto asHex() const -> UnallocatedCString final;
    auto asHex(alloc::Default alloc) const -> CString final;
    auto at(const std::size_t position) const -> const std::byte& final;
    auto begin() const -> const_iterator final;
    auto Bytes() const noexcept -> ReadView final;
    auto cbegin() const -> const_iterator final;
    auto cend() const -> const_iterator final;
    auto data() const -> const void* final;
    auto empty() const -> bool final;
    auto end() const -> const_iterator final;
    auto Extract(
        const std::size_t amount,
        Data& output,
        const std::size_t pos = 0) const -> bool final;
    auto Extract(std::uint8_t& output, const std::size_t pos = 0) const
        -> bool final;
    auto Extract(std::uint16_t& output, const std::size_t pos = 0) const
        -> bool final;
    auto Extract(std::uint32_t& output, const std::size_t pos = 0) const
        -> bool final;
    auto Extract(std::uint64_t& output, const std::size_t pos = 0) const
        -> bool final;
    auto get_allocator() const noexcept -> allocator_type final;
    auto GetString(const api::Crypto& api, String& theStr) const noexcept
        -> void;
    auto IsNull() const -> bool final;
    auto operator!=(const Data& rhs) const noexcept -> bool final;
    auto operator!=(const Generic& rhs) const noexcept -> bool;
    auto operator<(const Data& rhs) const noexcept -> bool final;
    auto operator<(const Generic& rhs) const noexcept -> bool;
    auto operator<=(const Data& rhs) const noexcept -> bool final;
    auto operator<=(const Generic& rhs) const noexcept -> bool;
    auto operator==(const Data& rhs) const noexcept -> bool final;
    auto operator==(const Generic& rhs) const noexcept -> bool;
    auto operator>(const Data& rhs) const noexcept -> bool final;
    auto operator>(const Generic& rhs) const noexcept -> bool;
    auto operator>=(const Data& rhs) const noexcept -> bool final;
    auto operator>=(const Generic& rhs) const noexcept -> bool;
    auto size() const -> std::size_t final;
    auto Type() const noexcept -> identifier::Type;

    auto Assign(const Data& source) noexcept -> bool final;
    auto Assign(const ReadView source) noexcept -> bool final;
    auto Assign(const void* data, const std::size_t size) noexcept
        -> bool final;
    auto at(const std::size_t position) -> std::byte& final;
    auto begin() -> iterator final;
    auto clear() noexcept -> void final;
    auto Concatenate(const ReadView) noexcept -> bool final;
    auto Concatenate(const void*, const std::size_t) noexcept -> bool final;
    auto data() -> void* final;
    auto DecodeHex(const ReadView hex) -> bool final;
    auto end() -> iterator final;
    auto operator+=(const Data& rhs) noexcept -> Generic& final;
    auto operator+=(const ReadView rhs) noexcept -> Generic& final;
    auto operator+=(const std::uint8_t rhs) noexcept -> Generic& final;
    auto operator+=(const std::uint16_t rhs) noexcept -> Generic& final;
    auto operator+=(const std::uint32_t rhs) noexcept -> Generic& final;
    auto operator+=(const std::uint64_t rhs) noexcept -> Generic& final;
    auto Randomize(const std::size_t size) -> bool final;
    auto resize(const std::size_t) -> bool final;
    auto Serialize(proto::Identifier& out) const noexcept -> bool;
    auto SetSize(const std::size_t) -> bool final;
    auto swap(Generic& rhs) noexcept -> void;
    auto WriteInto() noexcept -> AllocateOutput final;
    auto zeroMemory() -> void final;

    OPENTXS_NO_EXPORT Generic(Imp* imp) noexcept;
    Generic(allocator_type alloc = {}) noexcept;
    Generic(const Generic& rhs, allocator_type alloc = {}) noexcept;
    Generic(Generic&& rhs) noexcept;
    Generic(Generic&& rhs, allocator_type alloc) noexcept;
    auto operator=(const Generic& rhs) noexcept -> Generic&;
    auto operator=(Generic&& rhs) noexcept -> Generic&;

    ~Generic() override;

private:
    Imp* imp_;
};
}  // namespace opentxs::identifier
