// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <iosfwd>
#include <memory>

#include "blockchain/bitcoin/Inventory.hpp"
#include "blockchain/bitcoin/p2p/Message.hpp"
#include "internal/blockchain/p2p/bitcoin/message/Message.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
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
namespace p2p
{
namespace bitcoin
{
class Header;
}  // namespace bitcoin
}  // namespace p2p
}  // namespace blockchain
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::p2p::bitcoin::message::implementation
{
class Getdata final : public internal::Getdata, public implementation::Message
{
public:
    auto at(const std::size_t position) const noexcept(false)
        -> const value_type& final
    {
        return payload_.at(position);
    }
    auto begin() const noexcept -> const_iterator final { return {this, 0}; }
    auto end() const noexcept -> const_iterator final
    {
        return {this, payload_.size()};
    }
    using implementation::Message::payload;
    auto payload(AllocateOutput out) const noexcept -> bool final;
    auto size() const noexcept -> std::size_t final { return payload_.size(); }

    Getdata(
        const api::Session& api,
        const blockchain::Type network,
        UnallocatedVector<value_type>&& payload) noexcept;
    Getdata(
        const api::Session& api,
        std::unique_ptr<Header> header,
        UnallocatedVector<value_type>&& payload) noexcept;
    Getdata(const Getdata&) = delete;
    Getdata(Getdata&&) = delete;
    auto operator=(const Getdata&) -> Getdata& = delete;
    auto operator=(Getdata&&) -> Getdata& = delete;

    ~Getdata() final = default;

private:
    const UnallocatedVector<value_type> payload_;
};
}  // namespace opentxs::blockchain::p2p::bitcoin::message::implementation
