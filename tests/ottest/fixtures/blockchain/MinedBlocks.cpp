// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/MinedBlocks.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <mutex>
#include <stdexcept>

#include "internal/util/Mutex.hpp"

namespace ottest
{
struct MinedBlocks::Imp {
    using Vector = ot::UnallocatedVector<Future>;

    mutable std::mutex lock_{};
    Vector hashes_{};
};
}  // namespace ottest

namespace ottest
{
MinedBlocks::MinedBlocks()
    : imp_(std::make_unique<Imp>())
{
}

auto MinedBlocks::allocate() noexcept -> Promise
{
    auto lock = ot::Lock{imp_->lock_};
    auto promise = Promise{};
    imp_->hashes_.emplace_back(promise.get_future());

    return promise;
}

auto MinedBlocks::get(const std::size_t index) const -> Future
{
    auto lock = ot::Lock{imp_->lock_};

    if (index >= imp_->hashes_.size()) {
        throw std::out_of_range("Invalid index");
    }

    return imp_->hashes_.at(index);
}

MinedBlocks::~MinedBlocks() = default;
}  // namespace ottest
