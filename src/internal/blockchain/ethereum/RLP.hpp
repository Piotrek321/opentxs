// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

#include "opentxs/core/ByteArray.hpp"
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

namespace opentxs::blockchain::ethereum::rlp
{
using Null = std::monostate;
using String = ByteArray;
using Sequence = Vector<Node>;
using Data = std::variant<Null, String, Sequence>;

class Node
{
public:
    Data data_;

    /// throws std::invalid_argument if the input can not be parsed
    static auto Decode(const api::Session& api, ReadView serialized) noexcept(
        false) -> Node;

    auto Encode(const api::Session& api, AllocateOutput out) const noexcept
        -> bool;
    auto EncodedSize(const api::Session& api) const noexcept -> std::size_t;
    auto operator==(const Node& rhs) const noexcept -> bool;

    Node() noexcept;
    Node(Data&& data) noexcept;
    Node(Null&& value) noexcept;
    Node(Sequence&& value) noexcept;
    Node(String&& value) noexcept;
    Node(const Node& rhs) noexcept;
    Node(Node&& rhs) noexcept;
    auto operator=(const Node& rhs) noexcept -> Node&;
    auto operator=(Node&& rhs) noexcept -> Node&;

private:
    struct Calculator;
    struct Constants;
    struct Decoder;
    struct Encoder;

    friend Calculator;
    friend Encoder;

    auto Encode(Encoder& visitor) const noexcept -> bool;
    auto EncodedSize(const Calculator& visitor) const noexcept -> std::size_t;
};
}  // namespace opentxs::blockchain::ethereum::rlp
