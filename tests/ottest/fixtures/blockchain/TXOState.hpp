// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <opentxs/opentxs.hpp>

#include "ottest/Basic.hpp"

namespace ottest
{
struct TXOState {
    struct Data {
        ot::blockchain::Balance balance_;
        ot::UnallocatedMap<
            ot::blockchain::node::TxoState,
            ot::UnallocatedSet<ot::blockchain::block::Outpoint>>
            data_;

        Data() noexcept;
    };

    struct NymData {
        Data nym_;
        ot::UnallocatedMap<ot::OTIdentifier, Data> accounts_;

        NymData() noexcept;
    };

    Data wallet_;
    ot::UnallocatedMap<ot::OTNymID, NymData> nyms_;

    TXOState() noexcept;
};
}  // namespace ottest
