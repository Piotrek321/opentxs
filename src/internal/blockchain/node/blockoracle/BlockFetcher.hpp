// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/smart_ptr/shared_ptr.hpp>
#include <cstddef>

#include "opentxs/blockchain/Types.hpp"
#include "opentxs/util/Allocated.hpp"

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
namespace database
{
class Block;
}  // namespace database

namespace node
{
namespace internal
{
class BlockBatch;
}  // namespace internal

class HeaderOracle;
struct Endpoints;
}  // namespace node
}  // namespace blockchain
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::blockoracle
{
class BlockFetcher final : public Allocated
{
public:
    class Imp;

    auto get_allocator() const noexcept -> allocator_type final;
    auto GetJob(allocator_type alloc) const noexcept -> internal::BlockBatch;

    auto Shutdown() noexcept -> void;
    auto Start() noexcept -> void;

    BlockFetcher(
        const api::Session& api,
        const Endpoints& endpoints,
        const HeaderOracle& header,
        database::Block& db,
        blockchain::Type chain,
        std::size_t peerTarget) noexcept;
    BlockFetcher(const BlockFetcher&) noexcept;
    BlockFetcher(BlockFetcher&&) noexcept;
    auto operator=(const BlockFetcher&) noexcept -> BlockFetcher&;
    auto operator=(BlockFetcher&&) noexcept -> BlockFetcher&;

    ~BlockFetcher() final;

private:
    // TODO switch to std::shared_ptr once the android ndk ships a version of
    // libc++ with unfucked pmr / allocate_shared support
    boost::shared_ptr<Imp> imp_;
};
}  // namespace opentxs::blockchain::node::blockoracle
