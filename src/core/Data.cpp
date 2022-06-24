// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "core/Data.hpp"   // IWYU pragma: associated

extern "C" {
#include <sodium.h>
}

#include <boost/endian/buffers.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

#include "internal/core/Core.hpp"
#include "internal/util/P0330.hpp"
#include "opentxs/core/Armored.hpp"
#include "opentxs/core/Data.hpp"

namespace opentxs
{
auto check_subset(
    const std::size_t size,
    const std::size_t target,
    const std::size_t pos) noexcept -> bool
{
    if (pos > size) { return false; }

    if ((std::numeric_limits<std::size_t>::max() - pos) < target) {
        return false;
    }

    if ((pos + target) > size) { return false; }

    return true;
}

auto to_hex(const std::byte* in, std::size_t size) noexcept
    -> UnallocatedCString
{
    if (nullptr == in) { return {}; }

    auto out = std::stringstream{};

    for (auto i = 0_uz; i < size; ++i, ++in) {
        out << std::hex << std::setfill('0') << std::setw(2)
            << std::to_integer<int>(*in);
    }

    return out.str();
}

auto to_hex(
    const std::byte* in,
    std::size_t size,
    alloc::Resource* alloc) noexcept -> CString
{
    if (nullptr == in) { return CString{alloc}; }

    auto out = std::stringstream{};  // TODO c++20 use allocator

    for (auto i = 0_uz; i < size; ++i, ++in) {
        out << std::hex << std::setfill('0') << std::setw(2)
            << std::to_integer<int>(*in);
    }

    return CString{alloc}.append(out.str());
}

namespace implementation
{
Data::Data() noexcept
    : data_()
{
}

Data::Data(const opentxs::Armored& source) noexcept
    : data_()
{
    if (source.Exists()) { source.GetData(*this); }
}

Data::Data(const void* data, std::size_t size) noexcept
    : data_(
          static_cast<const std::uint8_t*>(data),
          static_cast<const std::uint8_t*>(data) + size)
{
}

Data::Data(const Vector& v) noexcept
    : Data(Vector(v))
{
}

Data::Data(Vector&& v) noexcept
    : data_(std::move(v))
{
}

Data::Data(const UnallocatedVector<std::byte>& v) noexcept
    : data_(
          reinterpret_cast<const std::uint8_t*>(v.data()),
          reinterpret_cast<const std::uint8_t*>(v.data()) + v.size())
{
}

auto Data::operator==(const opentxs::Data& rhs) const noexcept -> bool
{
    return 0 == spaceship(rhs);
}

auto Data::operator!=(const opentxs::Data& rhs) const noexcept -> bool
{
    return 0 != spaceship(rhs);
}

auto Data::operator<(const opentxs::Data& rhs) const noexcept -> bool
{
    return 0 > spaceship(rhs);
}

auto Data::operator>(const opentxs::Data& rhs) const noexcept -> bool
{
    return 0 < spaceship(rhs);
}

auto Data::operator<=(const opentxs::Data& rhs) const noexcept -> bool
{
    return 0 >= spaceship(rhs);
}

auto Data::operator>=(const opentxs::Data& rhs) const noexcept -> bool
{
    return 0 <= spaceship(rhs);
}

auto Data::operator+=(const opentxs::Data& rhs) -> Data&
{
    concatenate(dynamic_cast<const Data&>(rhs).data_);

    return *this;
}

auto Data::operator+=(const ReadView rhs) -> Data&
{
    Concatenate(rhs);

    return *this;
}

auto Data::operator+=(const std::uint8_t rhs) -> Data&
{
    data_.emplace_back(rhs);

    return *this;
}

auto Data::operator+=(const std::uint16_t rhs) -> Data&
{
    const auto input = boost::endian::big_uint16_buf_t(rhs);
    Data temp(&input, sizeof(input));
    concatenate(temp.data_);

    return *this;
}

auto Data::operator+=(const std::uint32_t rhs) -> Data&
{
    const auto input = boost::endian::big_uint32_buf_t(rhs);
    Data temp(&input, sizeof(input));
    concatenate(temp.data_);

    return *this;
}

auto Data::operator+=(const std::uint64_t rhs) -> Data&
{
    const auto input = boost::endian::big_uint64_buf_t(rhs);
    Data temp(&input, sizeof(input));
    concatenate(temp.data_);

    return *this;
}

auto Data::asHex() const -> UnallocatedCString
{
    return to_hex(
        reinterpret_cast<const std::byte*>(data_.data()), data_.size());
}

auto Data::asHex(alloc::Resource* alloc) const -> CString
{
    return to_hex(
        reinterpret_cast<const std::byte*>(data_.data()), data_.size(), alloc);
}

auto Data::Assign(const void* data, const std::size_t size) noexcept -> bool
{
    auto rhs = [&]() -> Vector {
        if ((data == nullptr) || (size == 0)) {

            return {};
        } else {
            const auto* i = static_cast<const std::uint8_t*>(data);

            return {i, std::next(i, size)};
        }
    }();
    data_.swap(rhs);

    return true;
}

auto Data::check_sub(const std::size_t pos, const std::size_t target) const
    -> bool
{
    return check_subset(data_.size(), target, pos);
}

auto Data::concatenate(const Vector& data) -> void
{
    for (const auto& byte : data) {
        data_.emplace_back(reinterpret_cast<const std::uint8_t&>(byte));
    }
}

auto Data::Concatenate(const void* data, const std::size_t size) noexcept
    -> bool
{
    if ((size == 0) || (nullptr == data)) { return false; }

    Data temp(data, size);
    concatenate(temp.data_);

    return true;
}

auto Data::DecodeHex(const std::string_view hex) -> bool
{
    data_.clear();

    if (hex.empty()) { return true; }

    if (2 > hex.size()) { return false; }

    const auto prefix = hex.substr(0, 2);
    const auto stripped = (prefix == "0x" || prefix == "0X")
                              ? hex.substr(2, hex.size() - 2)
                              : hex;
    using namespace std::literals;
    // TODO c++20 use ranges to prevent unnecessary copy
    const auto padded = (0 == stripped.size() % 2)
                            ? CString{stripped}
                            : CString{"0"sv}.append(stripped);

    for (std::size_t i = 0; i < padded.length(); i += 2) {
        data_.emplace_back(static_cast<std::uint8_t>(
            strtol(padded.substr(i, 2).c_str(), nullptr, 16)));
    }

    return true;
}

auto Data::Extract(
    const std::size_t amount,
    opentxs::Data& output,
    const std::size_t pos) const -> bool
{
    if (false == check_sub(pos, amount)) { return false; }

    output.Assign(&data_.at(pos), amount);

    return true;
}

auto Data::Extract(std::uint8_t& output, const std::size_t pos) const -> bool
{
    if (false == check_sub(pos, sizeof(output))) { return false; }

    output = data_.at(pos);

    return true;
}

auto Data::Extract(std::uint16_t& output, const std::size_t pos) const -> bool
{
    if (false == check_sub(pos, sizeof(output))) { return false; }

    auto temp = boost::endian::big_uint16_buf_t();
    std::memcpy(&temp, &data_.at(pos), sizeof(temp));
    output = temp.value();

    return true;
}

auto Data::Extract(std::uint32_t& output, const std::size_t pos) const -> bool
{
    if (false == check_sub(pos, sizeof(output))) { return false; }

    auto temp = boost::endian::big_uint32_buf_t();
    std::memcpy(&temp, &data_.at(pos), sizeof(temp));
    output = temp.value();

    return true;
}

auto Data::Extract(std::uint64_t& output, const std::size_t pos) const -> bool
{
    if (false == check_sub(pos, sizeof(output))) { return false; }

    auto temp = boost::endian::big_uint64_buf_t();
    std::memcpy(&temp, &data_.at(pos), sizeof(temp));
    output = temp.value();

    return true;
}

auto Data::Initialize() -> void { data_.clear(); }

auto Data::IsNull() const -> bool
{
    if (data_.empty()) { return true; }

    for (const auto& byte : data_) {
        if (0 != byte) { return false; }
    }

    return true;
}

auto Data::Randomize(const std::size_t size) -> bool
{
    SetSize(size);

    if (size == 0) { return false; }

    ::randombytes_buf(data_.data(), size);

    return true;
}

auto Data::resize(const std::size_t size) -> bool
{
    data_.resize(size);

    return true;
}

auto Data::SetSize(const std::size_t size) -> bool
{
    clear();

    if (size > 0) { data_.assign(size, 0); }

    return true;
}

auto Data::spaceship(const opentxs::Data& rhs) const noexcept -> int
{
    const auto lSize = data_.size();
    const auto rSize = rhs.size();

    if ((0u == lSize) && (0u == rSize)) { return 0; }
    if (lSize < rSize) { return -1; }
    if (lSize > rSize) { return 1; }

    return std::memcmp(data_.data(), rhs.data(), data_.size());
}

auto Data::str() const -> UnallocatedCString
{
    return UnallocatedCString{Bytes()};
}

auto Data::str(alloc::Resource* alloc) const -> CString
{
    return CString{Bytes(), alloc};
}

auto Data::WriteInto() noexcept -> AllocateOutput
{
    return [this](const auto size) {
        data_.clear();
        data_.assign(size, 51);

        return WritableView{data_.data(), data_.size()};
    };
}

auto Data::zeroMemory() -> void
{
    ::sodium_memzero(data_.data(), data_.size());
}
}  // namespace implementation
}  // namespace opentxs
