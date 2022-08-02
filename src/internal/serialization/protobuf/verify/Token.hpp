// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <CashEnums.pb.h>
#include <cstdint>

#include "opentxs/Version.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace proto
{
class Token;
}  // namespace proto
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::proto
{
auto CheckProto_1(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_2(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_3(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_4(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_5(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_6(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_7(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_8(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_9(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_10(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_11(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_12(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_13(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_14(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_15(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_16(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_17(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_18(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_19(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
auto CheckProto_20(
    const Token& input,
    const bool silent,
    const CashType expectedType,
    const UnallocatedSet<TokenState>& expectedState,
    UnallocatedCString& totalValue,
    std::int64_t& validFrom,
    std::int64_t& validTo) -> bool;
}  // namespace opentxs::proto
