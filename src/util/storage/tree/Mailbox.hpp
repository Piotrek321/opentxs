// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <mutex>

#include "Proto.hpp"
#include "internal/util/Editor.hpp"
#include "opentxs/util/Container.hpp"
#include "serialization/protobuf/StorageNymList.pb.h"
#include "util/storage/tree/Node.hpp"

namespace opentxs
{
namespace storage
{
class Driver;
class Nym;
}  // namespace storage
}  // namespace opentxs

namespace opentxs::storage
{
class Mailbox final : public Node
{
private:
    friend Nym;

    void init(const UnallocatedCString& hash) final;
    auto save(const std::unique_lock<std::mutex>& lock) const -> bool final;
    auto serialize() const -> proto::StorageNymList;

    Mailbox(const Driver& storage, const UnallocatedCString& hash);
    Mailbox() = delete;
    Mailbox(const Mailbox&) = delete;
    Mailbox(Mailbox&&) = delete;
    auto operator=(const Mailbox&) -> Mailbox = delete;
    auto operator=(Mailbox&&) -> Mailbox = delete;

public:
    auto Load(
        const UnallocatedCString& id,
        UnallocatedCString& output,
        UnallocatedCString& alias,
        const bool checking) const -> bool;

    auto Delete(const UnallocatedCString& id) -> bool;
    auto Store(
        const UnallocatedCString& id,
        const UnallocatedCString& data,
        const UnallocatedCString& alias) -> bool;

    ~Mailbox() final = default;
};
}  // namespace opentxs::storage