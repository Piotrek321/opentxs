// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/util/BlockchainProfile.hpp"
#include "opentxs/util/Container.hpp"

namespace opentxs::blockchain::node::internal
{
struct Config {
    BlockchainProfile profile_{BlockchainProfile::desktop};
    bool provide_sync_server_{false};
    bool disable_wallet_{false};

    auto print() const noexcept -> UnallocatedCString;
};
}  // namespace opentxs::blockchain::node::internal
