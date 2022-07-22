// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                        // IWYU pragma: associated
#include "1_Internal.hpp"                      // IWYU pragma: associated
#include "network/zeromq/context/Context.hpp"  // IWYU pragma: associated

#include <zmq.h>
#include <atomic>
#include <cassert>
#include <memory>
#include <thread>
#include <utility>

#include "internal/network/zeromq/Factory.hpp"
#include "internal/network/zeromq/Handle.hpp"
#include "internal/network/zeromq/socket/Factory.hpp"
#include "network/zeromq/PairEventListener.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/Proxy.hpp"
#include "opentxs/network/zeromq/socket/Dealer.hpp"
#include "opentxs/network/zeromq/socket/Pair.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/network/zeromq/socket/Pull.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/network/zeromq/socket/Reply.hpp"
#include "opentxs/network/zeromq/socket/Request.hpp"
#include "opentxs/network/zeromq/socket/Router.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"

namespace opentxs::factory
{
auto ZMQContext() noexcept -> std::shared_ptr<network::zeromq::Context>
{
    using ReturnType = network::zeromq::implementation::Context;

    return std::make_shared<ReturnType>();
}
}  // namespace opentxs::factory

namespace opentxs::network::zeromq
{
auto GetBatchID() noexcept -> BatchID
{
    static auto counter = std::atomic<BatchID>{};

    return ++counter;
}

auto GetSocketID() noexcept -> SocketID
{
    static auto counter = std::atomic<SocketID>{};

    return ++counter;
}
}  // namespace opentxs::network::zeromq

namespace opentxs::network::zeromq::implementation
{
Context::Context() noexcept
    : context_([] {
        auto* context = ::zmq_ctx_new();
        assert(nullptr != context);
        assert(1 == ::zmq_has("curve"));

        const auto init =
            ::zmq_ctx_set(context, ZMQ_MAX_SOCKETS, max_sockets());

        assert(0 == init);

        return context;
    }())
    , pool_(std::nullopt)
    , shutdown_()
{
    assert(nullptr != context_);
}

Context::operator void*() const noexcept
{
    assert(nullptr != context_);

    return context_;
}

auto Context::Alloc(BatchID id) const noexcept -> alloc::Resource*
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->Alloc(id);
}

auto Context::BelongsToThreadPool(const std::thread::id id) const noexcept
    -> bool
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->BelongsToThreadPool(id);
}

auto Context::DealerSocket(
    const ListenCallback& callback,
    const socket::Direction direction,
    const std::string_view threadName) const noexcept -> OTZMQDealerSocket
{
    return OTZMQDealerSocket{factory::DealerSocket(
        *this, static_cast<bool>(direction), callback, threadName)};
}

auto Context::Init(std::shared_ptr<const zeromq::Context> me) noexcept -> void
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(false == pool.has_value());

    pool.emplace(std::move(me));

    assert(pool.has_value());
}

auto Context::max_sockets() noexcept -> int { return 32768; }

auto Context::MakeBatch(Vector<socket::Type>&& types) const noexcept
    -> internal::Handle
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->MakeBatch(std::move(types));
}

auto Context::MakeBatch(
    const BatchID preallocated,
    Vector<socket::Type>&& types) const noexcept -> internal::Handle
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->MakeBatch(preallocated, std::move(types));
}

auto Context::Modify(SocketID id, ModifyCallback cb) const noexcept -> void
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    pool->Modify(id, std::move(cb));
}

auto Context::PairEventListener(
    const PairEventCallback& callback,
    const int instance,
    const std::string_view threadName) const noexcept -> OTZMQSubscribeSocket
{
    return OTZMQSubscribeSocket(
        new class PairEventListener(*this, callback, instance, threadName));
}

auto Context::PairSocket(
    const zeromq::ListenCallback& callback,
    const std::string_view threadName) const noexcept -> OTZMQPairSocket
{
    return OTZMQPairSocket{
        factory::PairSocket(*this, callback, true, threadName)};
}

auto Context::PairSocket(
    const zeromq::ListenCallback& callback,
    const socket::Pair& peer,
    const std::string_view threadName) const noexcept -> OTZMQPairSocket
{
    return OTZMQPairSocket{
        factory::PairSocket(callback, peer, true, threadName)};
}

auto Context::PairSocket(
    const zeromq::ListenCallback& callback,
    const std::string_view endpoint,
    const std::string_view threadName) const noexcept -> OTZMQPairSocket
{
    return OTZMQPairSocket{
        factory::PairSocket(*this, callback, endpoint, threadName)};
}

auto Context::Pipeline(
    std::function<void(zeromq::Message&&)>&& callback,
    const std::string_view threadName,
    const EndpointArgs& subscribe,
    const EndpointArgs& pull,
    const EndpointArgs& dealer,
    const Vector<SocketData>& extra,
    const std::optional<BatchID>& preallocated,
    alloc::Default pmr) const noexcept -> zeromq::Pipeline
{
    return opentxs::factory::Pipeline(
        *this,
        std::move(callback),
        subscribe,
        pull,
        dealer,
        extra,
        threadName,
        preallocated,
        pmr);
}

auto Context::PreallocateBatch() const noexcept -> BatchID
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->PreallocateBatch();
}

auto Context::Proxy(
    socket::Socket& frontend,
    socket::Socket& backend,
    const std::string_view threadName) const noexcept -> OTZMQProxy
{
    return zeromq::Proxy::Factory(*this, frontend, backend, threadName);
}

auto Context::PublishSocket() const noexcept -> OTZMQPublishSocket
{
    return OTZMQPublishSocket{factory::PublishSocket(*this)};
}

auto Context::PullSocket(
    const socket::Direction direction,
    const std::string_view threadName) const noexcept -> OTZMQPullSocket
{
    return OTZMQPullSocket{
        factory::PullSocket(*this, static_cast<bool>(direction), threadName)};
}

auto Context::PullSocket(
    const ListenCallback& callback,
    const socket::Direction direction,
    const std::string_view threadName) const noexcept -> OTZMQPullSocket
{
    return OTZMQPullSocket{factory::PullSocket(
        *this, static_cast<bool>(direction), callback, threadName)};
}

auto Context::PushSocket(const socket::Direction direction) const noexcept
    -> OTZMQPushSocket
{
    return OTZMQPushSocket{
        factory::PushSocket(*this, static_cast<bool>(direction))};
}

auto Context::RawSocket(socket::Type type) const noexcept -> socket::Raw
{
    return factory::ZMQSocket(*this, type);
}

auto Context::ReplySocket(
    const ReplyCallback& callback,
    const socket::Direction direction,
    const std::string_view threadName) const noexcept -> OTZMQReplySocket
{
    return OTZMQReplySocket{factory::ReplySocket(
        *this, static_cast<bool>(direction), callback, threadName)};
}

auto Context::RequestSocket() const noexcept -> OTZMQRequestSocket
{
    return OTZMQRequestSocket{factory::RequestSocket(*this)};
}

auto Context::RouterSocket(
    const ListenCallback& callback,
    const socket::Direction direction,
    const std::string_view threadName) const noexcept -> OTZMQRouterSocket
{
    return OTZMQRouterSocket{factory::RouterSocket(
        *this, static_cast<bool>(direction), callback, threadName)};
}

auto Context::Start(
    BatchID id,
    StartArgs&& sockets,
    const std::string_view threadName) const noexcept -> internal::Thread*
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->Start(id, std::move(sockets), threadName);
}

auto Context::Stop(BatchID id) const noexcept -> void
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    pool->Stop(id);
}

auto Context::Stop() noexcept -> std::future<void>
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    pool->Shutdown();

    return shutdown_.get_future();
}

auto Context::SubscribeSocket(
    const ListenCallback& callback,
    const std::string_view threadName) const noexcept -> OTZMQSubscribeSocket
{
    return OTZMQSubscribeSocket{
        factory::SubscribeSocket(*this, callback, threadName)};
}

auto Context::Thread(BatchID id) const noexcept -> internal::Thread*
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->Thread(id);
}

auto Context::ThreadID(BatchID id) const noexcept -> std::thread::id
{
    auto handle = pool_.lock();
    auto& pool = *handle;

    assert(pool.has_value());

    return pool->ThreadID(id);
}

Context::~Context()
{
    if (nullptr != context_) {
        std::thread{[context = context_] {
            // NOTE neither of these functions should block forever but
            // sometimes they do anyway
            ::zmq_ctx_shutdown(context);
            ::zmq_ctx_term(context);
        }}.detach();
        context_ = nullptr;
        shutdown_.set_value();
    }
}
}  // namespace opentxs::network::zeromq::implementation
