// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string_view>

#include "internal/blockchain/node/Types.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Work.hpp"

namespace opentxs::network::blockchain
{
// WARNING update print function if new values are added or removed
enum class PeerJob : OTZMQWorkType {
    shutdown = value(WorkType::Shutdown),
    blockbatch = value(WorkType::BlockchainBlockDownloadQueue),
    mempool = value(WorkType::BlockchainMempoolUpdated),
    registration = value(WorkType::AsioRegister),
    connect = value(WorkType::AsioConnect),
    disconnect = value(WorkType::AsioDisconnect),
    sendresult = value(WorkType::AsioSendResult),
    p2p = value(WorkType::BitcoinP2P),
    getheaders = value(opentxs::blockchain::node::PeerManagerJobs::Getheaders),
    getblock = value(opentxs::blockchain::node::PeerManagerJobs::Getblock),
    broadcasttx =
        value(opentxs::blockchain::node::PeerManagerJobs::BroadcastTransaction),
    broadcastblock =
        value(opentxs::blockchain::node::PeerManagerJobs::BroadcastBlock),
    jobavailablecfheaders = value(
        opentxs::blockchain::node::PeerManagerJobs::JobAvailableCfheaders),
    jobavailablecfilters =
        value(opentxs::blockchain::node::PeerManagerJobs::JobAvailableCfilters),
    jobavailableblock =
        value(opentxs::blockchain::node::PeerManagerJobs::JobAvailableBlock),
    dealerconnected = OT_ZMQ_INTERNAL_SIGNAL + 120,
    jobtimeout = OT_ZMQ_INTERNAL_SIGNAL + 121,
    needpeers = OT_ZMQ_INTERNAL_SIGNAL + 122,
    statetimeout = OT_ZMQ_INTERNAL_SIGNAL + 123,
    activitytimeout = OT_ZMQ_INTERNAL_SIGNAL + 124,
    needping = OT_ZMQ_INTERNAL_SIGNAL + 125,
    body = OT_ZMQ_INTERNAL_SIGNAL + 126,
    header = OT_ZMQ_INTERNAL_SIGNAL + 127,
    init = OT_ZMQ_INIT_SIGNAL,
    heartbeat = OT_ZMQ_HEARTBEAT_SIGNAL,
    statemachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
};

auto print(PeerJob) noexcept -> std::string_view;
}  // namespace opentxs::network::blockchain
