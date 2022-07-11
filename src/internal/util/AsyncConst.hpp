// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <future>

namespace opentxs
{
template <typename T>
class AsyncConst
{
public:
    auto get() const -> const T& { return future_.get(); }

    template <typename... Args>
    auto set_value(Args&&... args) -> void
    {
        promise_.set_value(std::forward<Args>(args)...);
    }

    AsyncConst() noexcept
        : promise_()
        , future_(promise_.get_future())
    {
    }
    AsyncConst(const AsyncConst&) = delete;
    AsyncConst(AsyncConst&&) = delete;
    auto operator=(const AsyncConst&) -> AsyncConst& = delete;
    auto operator=(AsyncConst&&) -> AsyncConst& = delete;

    ~AsyncConst() = default;

private:
    std::promise<const T> promise_;
    std::shared_future<const T> future_;
};
}  // namespace opentxs
