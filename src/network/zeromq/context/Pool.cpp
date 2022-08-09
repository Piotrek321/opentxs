// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                     // IWYU pragma: associated
#include "1_Internal.hpp"                   // IWYU pragma: associated
#include "network/zeromq/context/Pool.hpp"  // IWYU pragma: associated

#include <zmq.h>  // IWYU pragma: keep
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "internal/network/zeromq/Batch.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/Handle.hpp"
#include "internal/network/zeromq/Types.hpp"
#include "internal/network/zeromq/socket/Factory.hpp"
#include "internal/util/BoostPMR.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/P0330.hpp"
#include "network/zeromq/context/Thread.hpp"
#include "opentxs/network/zeromq/ZeroMQ.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"
#include "opentxs/util/Container.hpp"
#include "util/ScopeGuard.hpp"
#include "util/Work.hpp"

namespace opentxs::network::zeromq::context
{
Pool::Pool(std::shared_ptr<const Context> parent) noexcept
    : parent_p_(parent)
    , parent_(*parent_p_)
    , count_(std::thread::hardware_concurrency())
    , shutdown_counter_()
    , running_(true)
    , gate_()
    , notify_()
    , threads_()
    , batches_()
    , index_()
    , start_args_()
    , stop_args_()
    , modify_args_()
{
    for (unsigned int n{0}; n < count_; ++n) {
        auto [i, rc] = notify_.try_emplace(
            n,
            std::make_pair(
                MakeArbitraryInproc({}),
                parent_.Internal().RawSocket(socket::Type::Push)));

        assert(rc);

        auto& [endpoint, socket] = i->second;
        rc = socket.lock()->Bind(endpoint.c_str());

        assert(rc);

        threads_.try_emplace(n, n, *this, endpoint);
    }
}

auto Pool::Alloc(BatchID id) noexcept -> alloc::Resource*
{
    return get(id).Alloc();
}

auto Pool::BelongsToThreadPool(const std::thread::id id) const noexcept -> bool
{
    auto alloc = alloc::BoostMonotonic{1024};
    auto threads = Set<std::thread::id>{&alloc};

    for (const auto& [tid, thread] : threads_) { threads.emplace(thread.ID()); }

    return 0_uz < threads.count(id);
}

auto Pool::DoModify(SocketID id) noexcept -> void
{
    const auto ticket = gate_.get();

    if (ticket) { return; }

    auto null = factory::ZMQSocketNull();

    if (null.ID() == id) {
        auto callbackHandle = modify_args_.lock();

        try {
            auto& callbacks = callbackHandle->at(id);

            for (const auto& callback : callbacks) {
                try {
                    assert(callback);

                    callback(null);
                } catch (const std::exception& e) {
                    std::cerr << OT_PRETTY_CLASS() << e.what() << std::endl;
                }
            }

            callbacks.clear();
        } catch (...) {
        }
    } else {
        const auto indexHandle = index_.lock_shared();
        const auto& [bIndex, sIndex] = *indexHandle;
        auto callbackHandle = modify_args_.lock();

        try {
            auto* socket = sIndex.at(id).second;
            auto& callbacks = callbackHandle->at(id);

            for (const auto& callback : callbacks) {
                try {
                    assert(callback);

                    callback(*socket);
                } catch (const std::exception& e) {
                    std::cerr << OT_PRETTY_CLASS() << e.what() << std::endl;
                }
            }

            callbacks.clear();
        } catch (...) {
        }
    }
}

auto Pool::GetStartArgs(BatchID id) noexcept -> ThreadStartArgs
{
    auto args = [&] {
        auto handle = start_args_.lock();
        auto& map = *handle;
        auto i = map.find(id);

        assert(map.end() != i);

        auto post = ScopeGuard{[&] { map.erase(i); }};

        return std::move(i->second);
    }();
    auto sockets = [&] {
        auto out = ThreadStartArgs{};
        std::transform(
            args.begin(), args.end(), std::back_inserter(out), [](auto& val) {
                auto& [sID, socket, cb] = val;

                return std::make_pair(socket, std::move(cb));
            });

        return out;
    }();

    return sockets;
}

auto Pool::GetStopArgs(BatchID id) noexcept -> Set<void*>
{
    auto args = [&] {
        auto handle = stop_args_.lock();
        auto& map = *handle;
        auto i = map.find(id);

        assert(map.end() != i);

        auto post = ScopeGuard{[&] { map.erase(i); }};

        return std::move(i->second);
    }();
    stop_batch(id);

    return args;
}

auto Pool::get(BatchID id) const noexcept -> const context::Thread&
{
    return threads_.at(id % count_);
}

auto Pool::get(BatchID id) noexcept -> context::Thread&
{
    return threads_.at(id % count_);
}

auto Pool::MakeBatch(Vector<socket::Type>&& types) noexcept -> internal::Handle
{
    return MakeBatch(GetBatchID(), std::move(types));
}

auto Pool::MakeBatch(const BatchID id, Vector<socket::Type>&& types) noexcept
    -> internal::Handle
{
    Batches::iterator it;
    auto added{false};
    std::pair<Batches::iterator&, bool&> result{it, added};
    batches_.modify([&](auto& batches) {
        result = batches.try_emplace(
            id,
            std::make_shared<internal::Batch>(id, parent_, std::move(types)));
    });

    assert(added);

    auto& pBatch = it->second;

    assert(added);

    auto& batch = *pBatch;
    index_.modify([&](auto& index) {
        auto& [bIndex, sIndex] = index;
        auto& sockets = bIndex[batch.id_];

        assert(sockets.empty());

        sockets.reserve(batch.sockets_.size());

        for (auto& socket : batch.sockets_) {
            const auto sID = socket.ID();
            sockets.emplace_back(sID);

            assert(0_uz == sIndex.count(sID));

            const auto [i, rc] = sIndex.try_emplace(
                sID, std::make_pair(batch.id_, std::addressof(socket)));

            assert(rc);
        }
    });

    return {parent_p_, pBatch};
}

auto Pool::Modify(SocketID id, ModifyCallback cb) noexcept -> void
{
    const auto ticket = gate_.get();

    if (ticket) { return; }

    try {
        const auto batchID = index_.lock_shared()->socket_.at(id).first;
        {
            auto handle = modify_args_.lock();
            auto& map = *handle;
            map[id].emplace_back(std::move(cb));
        }
        auto& notify = socket(batchID);
        const auto rc = notify.lock()->Send([&] {
            auto out = MakeWork(Operation::change_socket);
            out.AddFrame(id);

            return out;
        }());

        if (false == rc) {

            throw std::runtime_error{"failed to queue socket modification"};
        }
    } catch (const std::exception& e) {
        std::cerr << OT_PRETTY_CLASS() << e.what() << std::endl;
    }
}

auto Pool::PreallocateBatch() const noexcept -> BatchID { return GetBatchID(); }

auto Pool::ReportShutdown(unsigned int index) noexcept -> void
{
    notify_.at(index).second.lock()->Close();

    if (++shutdown_counter_ == count_) { stop(); }
}

auto Pool::Shutdown() noexcept -> void
{
    if (auto running = running_.exchange(false); running) {
        gate_.shutdown();

        for (auto& [id, data] : notify_) {
            auto& socket = data.second;
            socket.lock()->Send(MakeWork(Operation::shutdown));
        }
    }
}

auto Pool::socket(BatchID id) noexcept -> GuardedSocket&
{
    return notify_.at(id % count_).second;
}

auto Pool::Start(
    BatchID id,
    StartArgs&& sockets,
    const std::string_view threadName) noexcept -> zeromq::internal::Thread*
{
    const auto ticket = gate_.get();

    if (ticket) { return nullptr; }

    try {
        {
            auto handle = start_args_.lock();
            auto& map = *handle;
            auto [i, rc] = map.try_emplace(id, std::move(sockets));

            if (false == rc) {
                throw std::runtime_error{"batch already added"};
            }
        }

        auto& thread = get(id);
        auto& notify = socket(id);
        const auto rc = notify.lock()->Send([&] {
            auto out = MakeWork(Operation::add_socket);
            out.AddFrame(id);
            out.AddFrame(threadName.data(), threadName.size());

            return out;
        }());

        if (rc) {

            return std::addressof(thread);
        } else {
            throw std::runtime_error{"failed to add batch to thread"};
        }
    } catch (const std::exception& e) {
        std::cerr << OT_PRETTY_CLASS() << e.what() << std::endl;

        return nullptr;
    }
}

auto Pool::Stop(BatchID id) noexcept -> void
{
    if (const auto ticket = gate_.get(); ticket) { return; }

    try {
        {
            auto sockets = [&] {
                auto out = Set<void*>{};
                auto handle = index_.lock_shared();
                const auto& [bIndex, sIndex] = *handle;

                for (const auto& sID : bIndex.at(id)) {
                    out.emplace(sIndex.at(sID).second->Native());
                }

                return out;
            }();
            auto handle = stop_args_.lock();
            auto& map = *handle;
            const auto [i, rc] = map.try_emplace(id, std::move(sockets));

            if (false == rc) {

                throw std::runtime_error{"failed queue socket list"};
            }
        }

        auto& notify = socket(id);
        const auto rc = notify.lock()->Send([&] {
            auto out = MakeWork(Operation::remove_socket);
            out.AddFrame(id);

            return out;
        }());

        if (false == rc) { throw std::runtime_error{"failed stop batch"}; }
    } catch (const std::exception& e) {
        std::cerr << OT_PRETTY_CLASS() << e.what() << std::endl;
    }
}

auto Pool::stop() noexcept -> void
{
    batches_.modify([](auto& map) { map.clear(); });
    index_.modify([](auto& index) { index.clear(); });
    start_args_.lock()->clear();
    stop_args_.lock()->clear();
    modify_args_.lock()->clear();
    threads_.clear();
    notify_.clear();
    parent_p_.reset();
}

auto Pool::stop_batch(BatchID id) noexcept -> void
{
    auto deletedSockets = Set<SocketID>{};
    index_.modify([&](auto& index) {
        auto& [bIndex, sIndex] = index;

        if (auto batch = bIndex.find(id); bIndex.end() != batch) {
            for (const auto& socketID : batch->second) {
                sIndex.erase(socketID);
                deletedSockets.emplace(socketID);
            }

            bIndex.erase(batch);
        }
    });
    {
        auto handle = modify_args_.lock();
        auto& map = *handle;

        for (const auto& socketID : deletedSockets) { map.erase(socketID); }
    }
    batches_.modify([&](auto& batch) { batch.erase(id); });
}

auto Pool::Thread(BatchID id) const noexcept -> zeromq::internal::Thread*
{
    auto& thread = static_cast<zeromq::internal::Thread&>(
        const_cast<Pool*>(this)->get(id));

    return &thread;
}

auto Pool::ThreadID(BatchID id) const noexcept -> std::thread::id
{
    return get(id).ID();
}

Pool::~Pool() = default;
}  // namespace opentxs::network::zeromq::context
