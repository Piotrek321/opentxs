// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                        // IWYU pragma: associated
#include "1_Internal.hpp"                      // IWYU pragma: associated
#include "network/blockchain/peer/HasJob.hpp"  // IWYU pragma: associated

namespace opentxs::network::blockchain::internal
{
auto Peer::Imp::HasJob::operator()(const std::monostate&) const noexcept -> bool
{
    return false;
}

auto Peer::Imp::HasJob::operator()(const GetHeadersJob&) const noexcept -> bool
{
    return true;
}

auto Peer::Imp::HasJob::operator()(
    const opentxs::blockchain::node::internal::BlockBatch&) const noexcept
    -> bool
{
    return true;
}

auto Peer::Imp::HasJob::operator()(
    const opentxs::blockchain::node::CfheaderJob&) const noexcept -> bool
{
    return true;
}

auto Peer::Imp::HasJob::operator()(
    const opentxs::blockchain::node::CfilterJob&) const noexcept -> bool
{
    return true;
}
}  // namespace opentxs::network::blockchain::internal
