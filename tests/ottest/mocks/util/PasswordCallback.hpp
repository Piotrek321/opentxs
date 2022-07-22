// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gmock/gmock.h>
#include <opentxs/opentxs.hpp>

namespace common::mocks::util
{
class PasswordCallbackMock : public opentxs::PasswordCallback
{
    MOCK_METHOD(
        void,
        runOne,
        (opentxs::Secret & output,
         std::string_view prompt,
         std::string_view key),
        (const, noexcept, override)){};
    MOCK_METHOD(
        void,
        runTwo,
        (opentxs::Secret & output,
         std::string_view prompt,
         std::string_view key),
        (const, noexcept, override)){};
};
}  // namespace common::mocks::util
