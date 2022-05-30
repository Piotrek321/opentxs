// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "network/blockchain/peer/Imp.hpp"  // IWYU pragma: associated

namespace opentxs::network::blockchain::internal
{
class Peer::Imp::UpdateCfheaderJob
{
public:
    auto operator()(std::monostate& job) noexcept -> JobUpdate;
    auto operator()(GetHeadersJob& job) noexcept -> JobUpdate;
    auto operator()(
        opentxs::blockchain::node::internal::BlockBatch& job) noexcept
        -> JobUpdate;
    auto operator()(opentxs::blockchain::node::CfheaderJob& job) noexcept
        -> JobUpdate;
    auto operator()(opentxs::blockchain::node::CfilterJob& job) noexcept
        -> JobUpdate;
    auto operator()(opentxs::blockchain::node::BlockJob& job) noexcept
        -> JobUpdate;

    UpdateCfheaderJob(
        opentxs::blockchain::cfilter::Type type,
        opentxs::blockchain::block::Position&& block,
        opentxs::blockchain::cfilter::Hash&& hash) noexcept;

private:
    opentxs::blockchain::block::Position block_;
    opentxs::blockchain::cfilter::Type type_;
    opentxs::blockchain::cfilter::Hash hash_;
};
}  // namespace opentxs::network::blockchain::internal
