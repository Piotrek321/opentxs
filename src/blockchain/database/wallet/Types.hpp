// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "internal/blockchain/database/Types.hpp"

#include "internal/blockchain/database/Wallet.hpp"

namespace opentxs::blockchain::database::wallet
{
using Parent = database::Wallet;
using SubchainIndex = Parent::SubchainIndex;
using NodeID = Parent::NodeID;
using Patterns = Parent::Patterns;
using ElementMap = Parent::ElementMap;
using MatchingIndices = Parent::MatchingIndices;
using PatternID = identifier::Generic;
}  // namespace opentxs::blockchain::database::wallet
