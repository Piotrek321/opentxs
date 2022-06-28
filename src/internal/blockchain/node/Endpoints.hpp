// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/util/Allocated.hpp"
#include "opentxs/util/Container.hpp"

namespace opentxs::blockchain::node
{
struct Endpoints final : public Allocated {
    const CString block_fetcher_job_ready_;

    auto get_allocator() const noexcept -> allocator_type final
    {
        return block_fetcher_job_ready_.get_allocator();
    }

    Endpoints(allocator_type alloc) noexcept;
    Endpoints() noexcept = delete;
    Endpoints(const Endpoints&) = delete;
    Endpoints(Endpoints&&) = delete;
    auto operator=(const Endpoints&) -> Endpoints& = delete;
    auto operator=(Endpoints&&) -> Endpoints& = delete;

    ~Endpoints() final = default;
};
}  // namespace opentxs::blockchain::node
