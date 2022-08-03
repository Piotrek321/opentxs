// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// TODO: MT-85: className to be changed
#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/crypto/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"

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
namespace block
{
class Outpoint;
}  // namespace block
}  // namespace blockchain

namespace identifier
{
class Generic;
}  // namespace identifier
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::block
{
using Subchain = blockchain::crypto::Subchain;
using SubchainID = std::pair<Subchain, identifier::Generic>;
using ElementID = std::pair<Bip32Index, SubchainID>;
using Pattern = std::pair<ElementID, Vector<std::byte>>;
using Patterns = Vector<Pattern>;
using Match = std::pair<pTxid, ElementID>;
using InputMatch = std::tuple<pTxid, Outpoint, ElementID>;
using InputMatches = UnallocatedVector<InputMatch>;
using OutputMatches = UnallocatedVector<Match>;
using Matches = std::pair<InputMatches, OutputMatches>;
using KeyID = blockchain::crypto::Key;
using ContactID = identifier::Generic;
using KeyData = UnallocatedMap<KeyID, std::pair<ContactID, ContactID>>;

struct ParsedPatterns {
    Vector<Vector<std::byte>> data_;
    UnallocatedMap<ReadView, Patterns::const_iterator> map_;

    ParsedPatterns(const Patterns& in) noexcept;
};
}  // namespace opentxs::blockchain::block

namespace opentxs::blockchain::block::internal
{
auto SetIntersection(
    const api::Session& api,
    const ReadView txid,
    const ParsedPatterns& patterns,
    const Vector<Vector<std::byte>>& compare) noexcept -> Matches;
}  // namespace opentxs::blockchain::block::internal
