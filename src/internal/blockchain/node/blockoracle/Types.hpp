// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string_view>

#include "opentxs/util/WorkType.hpp"
#include "util/Work.hpp"

namespace opentxs::blockchain::node::blockoracle
{
// WARNING update print function if new values are added or removed
enum class BlockFetcherJob : OTZMQWorkType {
    shutdown = value(WorkType::Shutdown),
    header = value(WorkType::BlockchainNewHeader),
    reorg = value(WorkType::BlockchainReorg),
    block_received = OT_ZMQ_INTERNAL_SIGNAL + 0u,
    batch_finished = OT_ZMQ_INTERNAL_SIGNAL + 1u,
    init = OT_ZMQ_INIT_SIGNAL,
    statemachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
};

auto print(BlockFetcherJob) noexcept -> std::string_view;
}  // namespace opentxs::blockchain::node::blockoracle
