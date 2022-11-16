// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"             // IWYU pragma: associated
#include "1_Internal.hpp"           // IWYU pragma: associated
#include "core/FixedByteArray.tpp"  // IWYU pragma: associated

namespace opentxs
{
#if defined(_WIN32) || defined(_WIN64)
// NOTE sorry Windows users, MSVC throws an ICE if we export this symbol
#else
template class FixedByteArray<32>;
#endif
}  // namespace opentxs
