// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                              // IWYU pragma: associated
#include "1_Internal.hpp"                            // IWYU pragma: associated
#include "opentxs/blockchain/node/HeaderOracle.hpp"  // IWYU pragma: associated

#include <mutex>

#include "internal/blockchain/Params.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::blockchain::node
{
auto HeaderOracle::GenesisBlockHash(const blockchain::Type type)
    -> const block::Hash&
{
    static std::mutex lock_{};
    static auto cache = UnallocatedMap<blockchain::Type, block::Hash>{};

    try {
        auto lock = Lock{lock_};
        {
            auto it = cache.find(type);

            if (cache.end() != it) { return it->second; }
        }

        const auto& data = params::Chains().at(type);
        block::Hash hash{};
        const auto rc = hash.DecodeHex(data.genesis_hash_hex_);
        OT_ASSERT(rc);

        const auto [it, added] = cache.emplace(type, hash);

        return it->second;
    } catch (...) {
        LogError()("opentxs::factory::")(__func__)(": Genesis hash not found")
            .Flush();

        throw;
    }
}
}  // namespace opentxs::blockchain::node
