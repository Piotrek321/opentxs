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
#include <stdexcept>
#include <thread>

#include "internal/util/LogMacros.hpp"
#include "network/zeromq/context/Thread.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/util/Container.hpp"

namespace opentxs::network::zeromq::context
{
Pool::Pool(const Context& parent) noexcept
    : parent_(parent)
    , count_(std::thread::hardware_concurrency())
    , gate_()
    , threads_()
    , batches_()
    , batch_index_()
    , socket_index_()
{
    for (unsigned int n{0}; n < count_; ++n) { threads_.try_emplace(n, *this); }
}

auto Pool::DoModify(SocketID id, ModifyCallback& cb) noexcept -> bool
{
    const auto ticket = gate_.get();

    if (ticket) { return false; }

    try {
        auto socket_index = socket_index_.lock_shared();
        auto [batch, socket] = socket_index->at(id);
        cb(*socket);

        return true;
    } catch (...) {

        return false;
    }
}

auto Pool::get(BatchID id) noexcept -> context::Thread&
{
    return threads_.at(id % count_);
}

auto Pool::MakeBatch(UnallocatedVector<socket::Type>&& types) noexcept
    -> internal::Batch&
{
    auto id = GetBatchID();
    Batches::iterator it;
    auto added{false};
    std::pair<Batches::iterator&, bool&> result{it, added};
    batches_.modify([&](auto& batches) {
        result = batches.try_emplace(id, id, parent_, std::move(types));
    });
    assert(added);

    return it->second;
}

auto Pool::Modify(SocketID id, ModifyCallback cb) noexcept -> AsyncResult
{
    const auto ticket = gate_.get();

    if (ticket) { return {}; }

    try {
        auto [batch, socket] = [&] {
            auto socket_index = socket_index_.lock_shared();
            return socket_index->at(id);
        }();

        return get(batch).Modify(id, std::move(cb));
    } catch (...) {

        return {};
    }
}

auto Pool::Shutdown() noexcept -> void { gate_.shutdown(); }

auto Pool::Start(BatchID id, StartArgs&& sockets) noexcept
    -> zeromq::internal::Thread*
{
    const auto ticket = gate_.get();

    if (ticket) { return nullptr; }

    try {

        if (auto batch_index = batch_index_.lock_shared();
            0u < batch_index->count(id)) {
            throw std::runtime_error{"batch already exists"};
        }

        auto& thread = get(id);

        if (thread.Add(id, std::move(sockets))) {

            return &thread;
        } else {
            throw std::runtime_error{"failed to add batch to thread"};
        }
    } catch (const std::exception& e) {
        std::cerr << OT_PRETTY_CLASS() << e.what() << std::endl;

        return nullptr;
    }
}

auto Pool::Stop(BatchID id) noexcept -> std::future<bool>
{
    try {
        auto sockets = [&] {
            auto out = UnallocatedVector<socket::Raw*>{};
            auto batch_index = batch_index_.lock_shared();
            for (const auto& sID : batch_index->at(id)) {
                auto socket_index = socket_index_.lock_shared();
                out.emplace_back(socket_index->at(sID).second);
            }

            return out;
        }();
        auto& thread = get(id);

        return thread.Remove(id, std::move(sockets));
    } catch (...) {
        auto promise = std::promise<bool>{};
        auto output = promise.get_future();
        promise.set_value(false);

        return output;
    }
}

auto Pool::UpdateIndex(BatchID id, StartArgs&& sockets) noexcept -> void
{
    for (auto& [sID, socket, cb] : sockets) {
        auto& sid = sID;
        auto& sock = socket;
        batch_index_.modify(
            [&](auto& batch_index) { batch_index[id].emplace_back(sid); });

        SocketIndex::iterator it;
        auto added{false};
        std::pair<SocketIndex::iterator&, bool&> result{it, added};
        socket_index_.modify([&](auto& socket_index) {
            assert(0u == socket_index.count(sid));
            result = socket_index.try_emplace(sid, std::make_pair(id, sock));
        });

        assert(added);
    }
}

auto Pool::UpdateIndex(BatchID id) noexcept -> void
{
    batch_index_.modify([&](auto& batch_index) {
        if (auto batch = batch_index.find(id); batch_index.end() != batch) {
            socket_index_.modify([&](auto& socket_index) {
                for (const auto& socketID : batch->second) {
                    socket_index.erase(socketID);
                }
            });
            batch_index.erase(batch);
        }
    });

    batches_.modify([&](auto& batch) { batch.erase(id); });
}

auto Pool::Thread(BatchID id) const noexcept -> zeromq::internal::Thread*
{
    auto& thread = static_cast<zeromq::internal::Thread&>(
        const_cast<Pool*>(this)->get(id));

    return &thread;
}

Pool::~Pool()
{
    gate_.shutdown();

    for (auto& [id, thread] : threads_) { thread.Shutdown(); }
}
}  // namespace opentxs::network::zeromq::context