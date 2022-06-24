// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <cstddef>
#include <string_view>

namespace ot = opentxs;

namespace ottest
{
using namespace std::literals;

static constexpr auto test_hex_{
    "6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000"sv};

TEST(FixedByteArray, default_construct)
{
    auto value = ot::FixedByteArray<32>{};

    EXPECT_EQ(value.size(), 32);
}

TEST(FixedByteArray, hex)
{
    auto value = ot::FixedByteArray<32>{};

    EXPECT_TRUE(value.DecodeHex(test_hex_));

    auto data = ot::ByteArray{};

    EXPECT_TRUE(data.DecodeHex(test_hex_));
    EXPECT_EQ(value, data);

    const auto fromFixed = value.asHex();
    const auto fromData = data.asHex();

    EXPECT_EQ(fromFixed, test_hex_);
    EXPECT_EQ(fromData, test_hex_);

    const auto value2 = ot::FixedByteArray<32>{value.Bytes()};

    EXPECT_EQ(value2, value);
}

TEST(FixedByteArray, extract_with_uint_overloads_check_values_and_corner_cases)
{
    auto value = ot::FixedByteArray<32>{};
    EXPECT_TRUE(value.DecodeHex(
        "0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"sv));

    std::uint8_t out8;
    EXPECT_FALSE(value.Extract(out8, 32));
    EXPECT_TRUE(value.Extract(out8, 0));
    EXPECT_EQ(out8, 0);

    EXPECT_TRUE(value.Extract(out8, 31));
    EXPECT_EQ(out8, 31);

    std::uint16_t out16;
    EXPECT_FALSE(value.Extract(out16, 31));
    EXPECT_TRUE(value.Extract(out16, 0));
    EXPECT_EQ(out16, 1);

    EXPECT_TRUE(value.Extract(out16, 30));
    EXPECT_EQ(out16, 7711);

    std::uint32_t out32;
    EXPECT_FALSE(value.Extract(out32, 29));
    EXPECT_TRUE(value.Extract(out32, 0));
    EXPECT_EQ(out32, 66051);

    EXPECT_TRUE(value.Extract(out32, 28));
    EXPECT_EQ(out32, 471670303);

    std::uint64_t out64;
    EXPECT_FALSE(value.Extract(out64, 25));
    EXPECT_TRUE(value.Extract(out64, 0));
    EXPECT_EQ(out64, 283686952306183ull);

    EXPECT_TRUE(value.Extract(out64, 24));
    EXPECT_EQ(out64, 1736447835066146335ull);
}

TEST(FixedByteArray, extract_with_data_overloads_check_values_and_corner_cases)
{
    auto value = ot::FixedByteArray<32>{};
    EXPECT_TRUE(value.DecodeHex(
        "0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"sv));

    auto data8 = ot::ByteArray{};
    EXPECT_FALSE(value.Extract(1, data8, 32));
    EXPECT_FALSE(value.Extract(100, data8, 32));
    EXPECT_FALSE(
        value.Extract(std::numeric_limits<std::size_t>::max(), data8, 32));
    EXPECT_TRUE(value.Extract(1, data8, 0));

    auto data16 = ot::ByteArray{};
    EXPECT_FALSE(value.Extract(2, data16, 31));
    EXPECT_TRUE(value.Extract(2, data16, 0));

    auto data32 = ot::ByteArray{};
    EXPECT_FALSE(value.Extract(4, data32, 29));
    EXPECT_TRUE(value.Extract(4, data32, 0));

    auto data64 = ot::ByteArray{};
    EXPECT_FALSE(value.Extract(8, data64, 25));
    EXPECT_TRUE(value.Extract(8, data64, 0));
}

}  // namespace ottest
