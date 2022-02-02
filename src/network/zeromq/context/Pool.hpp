// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <robin_hood.h>
#include <future>
#include <mutex>
#include <tuple>
#include <utility>

#include "internal/network/zeromq/Batch.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/Pool.hpp"
#include "internal/network/zeromq/Thread.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "network/zeromq/context/Thread.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "util/Gatekeeper.hpp"

#pragma once

namespace opentxs
{
namespace network
{
namespace zeromq
{
namespace context
{
class Thread;
}  // namespace context

namespace internal
{
class Batch;
class Thread;
}  // namespace internal

namespace socket
{
class Raw;
}  // namespace socket

class Context;
}  // namespace zeromq
}  // namespace network
}  // namespace opentxs

namespace opentxs::network::zeromq::context
{
class Pool final : public zeromq::internal::Pool
{
public:
    auto Parent() const noexcept -> const zeromq::Context& final
    {
        return parent_;
    }

    auto MakeBatch(UnallocatedVector<socket::Type>&& types) noexcept
        -> internal::Batch&;
    auto Modify(SocketID id, ModifyCallback cb) noexcept -> AsyncResult;
    auto DoModify(SocketID id, ModifyCallback& cb) noexcept -> bool final;
    auto Shutdown() noexcept -> void;
    auto Start(BatchID id, StartArgs&& sockets) noexcept
        -> zeromq::internal::Thread*;
    auto Thread(BatchID id) const noexcept -> zeromq::internal::Thread* final;
    auto Stop(BatchID id) noexcept -> std::future<bool>;
    auto UpdateIndex(BatchID id, StartArgs&& sockets) noexcept -> void final;
    auto UpdateIndex(BatchID id) noexcept -> void final;

    Pool(const Context& parent) noexcept;

    ~Pool() final;

private:
    const Context& parent_;
    const unsigned int count_;
    mutable std::mutex index_lock_;
    mutable std::mutex batch_lock_;
    Gatekeeper gate_;
    robin_hood::unordered_node_map<unsigned int, context::Thread> threads_;
    robin_hood::unordered_node_map<BatchID, internal::Batch> batches_;
    robin_hood::unordered_node_map<BatchID, UnallocatedVector<SocketID>>
        batch_index_;
    robin_hood::unordered_node_map<SocketID, std::pair<BatchID, socket::Raw*>>
        socket_index_;

    auto get(BatchID id) noexcept -> context::Thread&;

    Pool() = delete;
    Pool(const Pool&) = delete;
    Pool(Pool&&) = delete;
    auto operator=(const Pool&) -> Pool& = delete;
    auto operator=(Pool&&) -> Pool& = delete;
};
}  // namespace opentxs::network::zeromq::context
