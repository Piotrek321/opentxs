// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/blockchain/block/Hash.hpp"

#pragma once

#include <memory>
#include <string_view>

#include "blockchain/DownloadTask.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Types.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Blank.hpp"
#include "util/Work.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
class Session;
}  // namespace api

namespace blockchain
{
namespace bitcoin
{
namespace block
{
class Block;
}  // namespace block
}  // namespace bitcoin

namespace block
{
class Hash;
}  // namespace block

namespace cfilter
{
class Hash;
class Header;
}  // namespace cfilter

class GCS;
}  // namespace blockchain

template <typename T>
struct make_blank;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs
{
template <>
struct make_blank<blockchain::block::Height> {
    static auto value(const api::Session&) -> blockchain::block::Height
    {
        return -1;
    }
};
template <>
struct make_blank<blockchain::block::Position> {
    static auto value(const api::Session& api) -> blockchain::block::Position
    {
        return {
            make_blank<blockchain::block::Height>::value(api),
            blockchain::block::Hash{}};
    }
};
}  // namespace opentxs

namespace opentxs::blockchain::node
{
// WARNING update print function if new values are added or removed
enum class BlockOracleJobs : OTZMQWorkType {
    shutdown = value(WorkType::Shutdown),
    request_blocks = OT_ZMQ_INTERNAL_SIGNAL + 0,
    process_block = OT_ZMQ_INTERNAL_SIGNAL + 1,
    start_downloader = OT_ZMQ_INTERNAL_SIGNAL + 2,
    init = OT_ZMQ_INIT_SIGNAL,
    statemachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
};

enum class ManagerJobs : OTZMQWorkType {
    Shutdown = value(WorkType::Shutdown),
    SyncReply = value(WorkType::P2PBlockchainSyncReply),
    SyncNewBlock = value(WorkType::P2PBlockchainNewBlock),
    SubmitBlockHeader = OT_ZMQ_INTERNAL_SIGNAL + 0,
    SubmitBlock = OT_ZMQ_INTERNAL_SIGNAL + 2,
    Heartbeat = OT_ZMQ_INTERNAL_SIGNAL + 3,
    SendToAddress = OT_ZMQ_INTERNAL_SIGNAL + 4,
    SendToPaymentCode = OT_ZMQ_INTERNAL_SIGNAL + 5,
    StartWallet = OT_ZMQ_INTERNAL_SIGNAL + 6,
    FilterUpdate = OT_ZMQ_NEW_FILTER_SIGNAL,
    StateMachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
};

enum class PeerManagerJobs : OTZMQWorkType {
    Getheaders = OT_ZMQ_INTERNAL_SIGNAL + 0,
    Getblock = OT_ZMQ_INTERNAL_SIGNAL + 1,
    BroadcastTransaction = OT_ZMQ_INTERNAL_SIGNAL + 2,
    JobAvailableCfheaders = OT_ZMQ_INTERNAL_SIGNAL + 4,
    JobAvailableCfilters = OT_ZMQ_INTERNAL_SIGNAL + 5,
    JobAvailableBlock = OT_ZMQ_INTERNAL_SIGNAL + 6,
    Heartbeat = OT_ZMQ_HEARTBEAT_SIGNAL,
};

using CfheaderJob =
    download::Batch<cfilter::Hash, cfilter::Header, cfilter::Type>;
using CfilterJob = download::Batch<GCS, cfilter::Header, cfilter::Type>;

auto print(BlockOracleJobs) noexcept -> std::string_view;
constexpr auto value(PeerManagerJobs job) noexcept
{
    return static_cast<OTZMQWorkType>(job);
}
}  // namespace opentxs::blockchain::node
