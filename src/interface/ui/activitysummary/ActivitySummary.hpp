// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <utility>

#include "1_Internal.hpp"
#include "interface/ui/base/List.hpp"
#include "interface/ui/base/Widget.hpp"
#include "internal/interface/ui/UI.hpp"
#include "internal/util/Flag.hpp"
#include "internal/util/Lockable.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/interface/ui/ActivitySummary.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/SharedPimpl.hpp"

namespace opentxs
{
namespace api
{
namespace session
{
class Client;
}  // namespace session
}  // namespace api

namespace identifier
{
class Nym;
}  // namespace identifier

namespace network
{
namespace zeromq
{
namespace socket
{
class Publish;
}  // namespace socket

class Message;
}  // namespace zeromq
}  // namespace network

namespace proto
{
class StorageThread;
class StorageThreadItem;
}  // namespace proto

class Flag;
class Identifier;
}  // namespace opentxs

namespace opentxs::ui::implementation
{
using ActivitySummaryList = List<
    ActivitySummaryExternalInterface,
    ActivitySummaryInternalInterface,
    ActivitySummaryRowID,
    ActivitySummaryRowInterface,
    ActivitySummaryRowInternal,
    ActivitySummaryRowBlank,
    ActivitySummarySortKey,
    ActivitySummaryPrimaryID>;

class ActivitySummary final : public ActivitySummaryList
{
public:
    ActivitySummary(
        const api::session::Client& api,
        const Flag& running,
        const identifier::Nym& nymID,
        const SimpleCallback& cb) noexcept;

    ~ActivitySummary() final;

private:
    const ListenerDefinitions listeners_;
    const Flag& running_;

    static auto newest_item(
        const Identifier& id,
        const proto::StorageThread& thread,
        CustomData& custom) noexcept -> const proto::StorageThreadItem&;

    auto construct_row(
        const ActivitySummaryRowID& id,
        const ActivitySummarySortKey& index,
        CustomData& custom) const noexcept -> RowPointer final;
    auto display_name(const proto::StorageThread& thread) const noexcept
        -> UnallocatedCString;

    void process_thread(const UnallocatedCString& threadID) noexcept;
    void process_thread(const Message& message) noexcept;
    void startup() noexcept;

    ActivitySummary() = delete;
    ActivitySummary(const ActivitySummary&) = delete;
    ActivitySummary(ActivitySummary&&) = delete;
    auto operator=(const ActivitySummary&) -> ActivitySummary& = delete;
    auto operator=(ActivitySummary&&) -> ActivitySummary& = delete;
};
}  // namespace opentxs::ui::implementation