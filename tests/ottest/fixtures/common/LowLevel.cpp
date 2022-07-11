// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/common/LowLevel.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <utility>

#include "ottest/Basic.hpp"

namespace ottest
{
LowLevel::LowLevel(const ot::Options& args) noexcept
    : caller_()
    , password_()
    , ot_([&]() -> auto& {
        caller_.SetCallback(std::addressof(password_));

        return ot::InitContext(args, std::addressof(caller_));
    }())
{
}

LowLevel::LowLevel() noexcept
    : LowLevel(Args(true))
{
}

LowLevel::~LowLevel() { ot::Cleanup(); }
}  // namespace ottest
