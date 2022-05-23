// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/common/OneClientSession.hpp"  // IWYU pragma: associated

namespace ottest
{
OneClientSession::OneClientSession() noexcept
    : client_1_(StartClient(0))
{
}
}  // namespace ottest
