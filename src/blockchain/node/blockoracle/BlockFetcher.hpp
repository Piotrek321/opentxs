// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/smart_ptr/shared_ptr.hpp>
#include <cs_plain_guarded.h>
#include <cstddef>
#include <optional>
#include <tuple>
#include <vector>

#include "internal/blockchain/node/Job.hpp"
#include "internal/blockchain/node/blockoracle/BlockFetcher.hpp"
#include "internal/blockchain/node/blockoracle/Types.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Time.hpp"
#include "util/Actor.hpp"

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
class Hash;
class Position;
}  // namespace block

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

namespace network
{
namespace zeromq
{
namespace socket
{
class Raw;
}  // namespace socket
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::blockoracle
{
class BlockFetcher::Imp final : public Actor<BlockFetcherJob>
{
public:
    auto GetJob(allocator_type alloc) const noexcept -> internal::BlockBatch;

    auto Init(boost::shared_ptr<Imp> self) noexcept -> void;
    auto Shutdown() noexcept -> void;

    Imp(const api::Session& api,
        const Endpoints& endpoints,
        const HeaderOracle& header,
        database::Block& db,
        blockchain::Type chain,
        std::size_t peerTarget,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept;
    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~Imp() final;

private:
    friend Actor<BlockFetcherJob>;

    enum class Status { pending, downloading, success };
    using Index =
        Map<block::Height,
            std::tuple<block::Hash, std::optional<download::JobID>, Status>>;

    struct Data : public Allocated {
        block::Position tip_;
        std::size_t queue_;
        Index blocks_;
        Map<download::JobID, Map<block::Hash, Index::iterator>> job_index_;

        auto get_allocator() const noexcept -> allocator_type final
        {
            return job_index_.get_allocator();
        }

        Data(allocator_type alloc) noexcept
            : tip_()
            , queue_()
            , blocks_(alloc)
            , job_index_(alloc)
        {
        }
        Data() = delete;
        Data(const Data&) = delete;
        Data(Data&&) = delete;
        auto operator=(const Data&) -> Data& = delete;
        auto operator=(Data&&) -> Data& = delete;
    };

    using Guarded = libguarded::plain_guarded<Data>;

    const api::Session& api_;
    const HeaderOracle& header_oracle_;
    database::Block& db_;
    network::zeromq::socket::Raw& job_ready_;
    network::zeromq::socket::Raw& tip_updated_;
    const blockchain::Type chain_;
    const std::size_t peer_target_;
    mutable Guarded data_;
    boost::shared_ptr<Imp> self_;

    auto broadcast_tip(const block::Position& tip) noexcept -> void;
    auto do_shutdown() noexcept -> void override;
    auto do_startup() noexcept -> void override;
    auto erase_obsolete(const block::Position& after, Data& data) noexcept
        -> void;
    auto pipeline(const Work work, Message&& msg) noexcept -> void override;
    auto process_batch_finished(Message&& msg) noexcept -> void;
    auto process_block_received(Message&& msg) noexcept -> void;
    auto process_reorg(Message&& msg) noexcept -> void;
    auto update_tip(Data& data) noexcept -> void;
    auto work() noexcept -> bool override;
};
}  // namespace opentxs::blockchain::node::blockoracle
