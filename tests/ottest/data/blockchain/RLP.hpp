// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <opentxs/opentxs.hpp>
#include <cstdint>
#include <string_view>

#include "internal/blockchain/ethereum/RLP.hpp"

namespace ot = opentxs;

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace boost
{
namespace json
{
class string;
class value;
}  // namespace json
}  // namespace boost

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
namespace ethereum
{
namespace rlp
{
class Node;
}  // namespace rlp
}  // namespace ethereum
}  // namespace blockchain
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace ottest
{
struct RLPVector {
    ot::CString name_{};
    ot::blockchain::ethereum::rlp::Node node_{};
    ot::OTData encoded_{ot::Data::Factory()};
};

auto GetRLPVectors(const ot::api::Session& api) noexcept
    -> const ot::Vector<RLPVector>&;
}  // namespace ottest

namespace ottest
{
auto get_rlp_raw() noexcept -> std::string_view;
auto json_is_bigint(const boost::json::string& in) noexcept -> bool;
auto json_is_escaped_unicode(const boost::json::string& in) noexcept -> bool;
auto parse(
    const opentxs::api::Session& api,
    const boost::json::string& in,
    opentxs::blockchain::ethereum::rlp::Node& out) noexcept -> void;
auto parse(
    const opentxs::api::Session& api,
    const boost::json::value& in,
    opentxs::blockchain::ethereum::rlp::Node& out) noexcept -> void;
auto parse(
    const opentxs::api::Session& api,
    std::int64_t in,
    opentxs::blockchain::ethereum::rlp::Node& out) noexcept -> void;
auto parse_as_bigint(
    const opentxs::api::Session& api,
    const boost::json::string& in,
    opentxs::blockchain::ethereum::rlp::Node& out) noexcept -> void;
auto parse_as_escaped_unicode(
    const opentxs::api::Session& api,
    const boost::json::string& in,
    opentxs::blockchain::ethereum::rlp::Node& out) noexcept -> void;
}  // namespace ottest
