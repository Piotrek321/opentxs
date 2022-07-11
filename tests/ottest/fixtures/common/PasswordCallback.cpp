// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/common/PasswordCallback.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>

namespace ottest
{
PasswordCallback::PasswordCallback() noexcept
    : ot::PasswordCallback()
    , password_(password_default_)
{
}

auto PasswordCallback::runOne(
    const char* szDisplay,
    ot::Secret& theOutput,
    const ot::UnallocatedCString& key) const -> void
{
    theOutput.AssignText(password_);
}

auto PasswordCallback::runTwo(
    const char* szDisplay,
    ot::Secret& theOutput,
    const ot::UnallocatedCString& key) const -> void
{
    theOutput.AssignText(password_);
}

auto PasswordCallback::SetPassword(std::string_view password) -> void
{
    password_ = password;
}
}  // namespace ottest
