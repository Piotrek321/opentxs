// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/filteroracle/FilterOracle.hpp"  // IWYU pragma: associated

#include "blockchain/node/filteroracle/FilterDownloader.hpp"
#include "internal/blockchain/database/Cfilter.hpp"
#include "internal/blockchain/node/Manager.hpp"

namespace opentxs::blockchain::node::implementation
{

auto FilterOracle::FilterDownloader::NextBatch() noexcept -> BatchType
{
    return allocate_batch(type_);
}


auto FilterOracle::FilterDownloader::process_reset(
    const zmq::Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(3 < body.size());

    auto position = Position{
        body.at(1).as<block::Height>(), block::Hash{body.at(2).Bytes()}};
    auto promise = std::promise<cfilter::Header>{};
    promise.set_value(body.at(3).Bytes());
    Reset(position, promise.get_future());
}
}  // namespace opentxs::blockchain::node::implementation
