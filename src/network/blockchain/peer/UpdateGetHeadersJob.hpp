// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "network/blockchain/peer/Imp.hpp"  // IWYU pragma: associated

namespace opentxs::network::blockchain::internal
{
class Peer::Imp::UpdateGetHeadersJob
{
public:
    auto operator()(std::monostate& job) const noexcept -> JobUpdate;
    auto operator()(GetHeadersJob& job) const noexcept -> JobUpdate;
    auto operator()(opentxs::blockchain::node::internal::BlockBatch& job)
        const noexcept -> JobUpdate;
    auto operator()(opentxs::blockchain::node::CfheaderJob& job) const noexcept
        -> JobUpdate;
    auto operator()(opentxs::blockchain::node::CfilterJob& job) const noexcept
        -> JobUpdate;
};
}  // namespace opentxs::network::blockchain::internal
