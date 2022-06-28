// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                            // IWYU pragma: associated
#include "1_Internal.hpp"                          // IWYU pragma: associated
#include "internal/blockchain/node/Endpoints.hpp"  // IWYU pragma: associated

#include "opentxs/network/zeromq/ZeroMQ.hpp"

namespace opentxs::blockchain::node
{
Endpoints::Endpoints(allocator_type alloc) noexcept
    : block_fetcher_job_ready_(network::zeromq::MakeArbitraryInproc(alloc))
{
}
}  // namespace opentxs::blockchain::node
