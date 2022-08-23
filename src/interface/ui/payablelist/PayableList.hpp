// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/core/UnitType.hpp"

#pragma once

#include <utility>

#include "1_Internal.hpp"
#include "Proto.hpp"
#include "core/Worker.hpp"
#include "interface/ui/base/List.hpp"
#include "interface/ui/base/Widget.hpp"
#include "internal/interface/ui/UI.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/core/Types.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/identity/wot/claim/ClaimType.hpp"
#include "opentxs/interface/ui/PayableList.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/SharedPimpl.hpp"
#include "opentxs/util/Types.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Work.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
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
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui::implementation
{
using PayableListList = List<
    PayableExternalInterface,
    PayableInternalInterface,
    PayableListRowID,
    PayableListRowInterface,
    PayableListRowInternal,
    PayableListRowBlank,
    PayableListSortKey,
    PayablePrimaryID>;

class PayableList final : public PayableListList,
                          public Worker<api::session::Client>
{
public:
    auto ID() const noexcept -> const Identifier& final
    {
        return owner_contact_id_;
    }

    PayableList(
        const api::session::Client& api,
        const identifier::Nym& nymID,
        const UnitType& currency,
        const SimpleCallback& cb) noexcept;
    PayableList() = delete;
    PayableList(const PayableList&) = delete;
    PayableList(PayableList&&) = delete;
    auto operator=(const PayableList&) -> PayableList& = delete;
    auto operator=(PayableList&&) -> PayableList& = delete;

    ~PayableList() final;

protected:
    auto pipeline(Message&& in) noexcept -> void final;
    auto state_machine() noexcept -> int final;
    auto last_job_str() const noexcept -> std::string final;

private:
    auto shut_down() noexcept -> void;

private:
    enum class Work : OTZMQWorkType {
        contact = value(WorkType::ContactUpdated),
        nym = value(WorkType::NymUpdated),
        init = OT_ZMQ_INIT_SIGNAL,
        statemachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
        shutdown = value(WorkType::Shutdown),
    };
    static auto to_str(Work) -> std::string;

    const OTIdentifier owner_contact_id_;
    const UnitType currency_;
    Work last_job_;

    auto construct_row(
        const PayableListRowID& id,
        const PayableListSortKey& index,
        CustomData& custom) const noexcept -> RowPointer final;
    auto last(const PayableListRowID& id) const noexcept -> bool final
    {
        return PayableListList::last(id);
    }

    auto process_contact(
        const PayableListRowID& id,
        const PayableListSortKey& key) noexcept -> void;
    auto process_contact(const Message& message) noexcept -> void;
    auto process_nym(const Message& message) noexcept -> void;
    auto startup() noexcept -> void;
};
}  // namespace opentxs::ui::implementation
