// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "network/blockchain/peer/Imp.hpp"  // IWYU pragma: associated

namespace opentxs::network::blockchain::internal
{
class Peer::Imp::RunJob
{
public:
    auto operator()(std::monostate& job) noexcept -> void;
    auto operator()(GetHeadersJob& job) noexcept -> void;
    auto operator()(
        opentxs::blockchain::node::internal::BlockBatch& job) noexcept -> void;
    auto operator()(opentxs::blockchain::node::CfheaderJob& job) noexcept
        -> void;
    auto operator()(opentxs::blockchain::node::CfilterJob& job) noexcept
        -> void;
    auto operator()(opentxs::blockchain::node::BlockJob& job) noexcept -> void;

    RunJob(Imp& parent) noexcept;

private:
    Imp& parent_;
};
}  // namespace opentxs::network::blockchain::internal
