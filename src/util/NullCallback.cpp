// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"           // IWYU pragma: associated
#include "1_Internal.hpp"         // IWYU pragma: associated
#include "util/NullCallback.hpp"  // IWYU pragma: associated

#include "2_Factory.hpp"
#include "internal/core/Factory.hpp"
#include "opentxs/core/Secret.hpp"

namespace opentxs
{
auto Factory::NullCallback() -> PasswordCallback*
{
    return new implementation::NullCallback();
}

auto DefaultPassword() noexcept -> const char*
{
    static constexpr auto password{"opentxs"};

    return password;
}
}  // namespace opentxs

namespace opentxs::implementation
{
auto NullCallback::runOne(
    opentxs::Secret& output,
    std::string_view prompt,
    std::string_view key) const noexcept -> void
{
    output.AssignText(DefaultPassword());
}

auto NullCallback::runTwo(
    opentxs::Secret& output,
    std::string_view prompt,
    std::string_view key) const noexcept -> void
{
    runOne(output, prompt, key);
}
}  // namespace opentxs::implementation
