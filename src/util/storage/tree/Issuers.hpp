// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <mutex>

#include "Proto.hpp"
#include "internal/util/Editor.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"
#include "serialization/protobuf/StorageIssuers.pb.h"
#include "util/storage/tree/Node.hpp"

namespace opentxs
{
namespace proto
{
class Issuer;
}  // namespace proto

namespace storage
{
class Driver;
class Nym;
}  // namespace storage
}  // namespace opentxs

namespace opentxs::storage
{
class Issuers final : public Node
{
public:
    auto Load(
        const UnallocatedCString& id,
        std::shared_ptr<proto::Issuer>& output,
        UnallocatedCString& alias,
        const bool checking) const -> bool;

    auto Delete(const UnallocatedCString& id) -> bool;
    auto Store(const proto::Issuer& data, const UnallocatedCString& alias)
        -> bool;

    ~Issuers() final = default;

private:
    friend Nym;

    static constexpr auto current_version_ = VersionNumber{1};

    void init(const UnallocatedCString& hash) final;
    auto save(const std::unique_lock<std::mutex>& lock) const -> bool final;
    auto serialize() const -> proto::StorageIssuers;

    Issuers(const Driver& storage, const UnallocatedCString& hash);
    Issuers() = delete;
    Issuers(const Issuers&) = delete;
    Issuers(Issuers&&) = delete;
    auto operator=(const Issuers&) -> Issuers = delete;
    auto operator=(Issuers&&) -> Issuers = delete;
};
}  // namespace opentxs::storage