// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/core/identifier/Algorithm.hpp"
// IWYU pragma: no_include "opentxs/core/identifier/Type.hpp"

#pragma once

#include <memory>

#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Types.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Bytes.hpp"

namespace opentxs::factory
{
auto Identifier(
    const identifier::Type type,
    identifier::Generic::allocator_type alloc) noexcept
    -> identifier::Generic::Imp*;
auto Identifier(
    const identifier::Type type,
    const identifier::Algorithm algorithm,
    const ReadView hash,
    identifier::Generic::allocator_type alloc) noexcept
    -> identifier::Generic::Imp*;
auto IdentifierInvalid(identifier::Generic::allocator_type alloc) noexcept
    -> identifier::Generic::Imp*;
}  // namespace opentxs::factory
