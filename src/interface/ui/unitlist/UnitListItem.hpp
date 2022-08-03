// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/core/UnitType.hpp"

#pragma once

#include <iosfwd>

#include "1_Internal.hpp"
#include "Proto.hpp"
#include "interface/ui/base/Row.hpp"
#include "internal/interface/ui/UI.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/core/Types.hpp"
#include "opentxs/core/contract/ServerContract.hpp"
#include "opentxs/core/contract/Unit.hpp"
#include "opentxs/identity/wot/claim/ClaimType.hpp"
#include "opentxs/interface/ui/UnitListItem.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/SharedPimpl.hpp"

class QVariant;

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

class Session;
}  // namespace api

namespace network
{
namespace zeromq
{
namespace socket
{
class Publish;
}  // namespace socket
}  // namespace zeromq
}  // namespace network

namespace ui
{
class UnitListItem;
}  // namespace ui
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui::implementation
{
using UnitListItemRow =
    Row<UnitListRowInternal, UnitListInternalInterface, UnitListRowID>;

class UnitListItem final : public UnitListItemRow
{
public:
    const api::session::Client& api_;

    auto Name() const noexcept -> UnallocatedCString final { return name_; }
    auto Unit() const noexcept -> UnitType final { return row_id_; }

    UnitListItem(
        const UnitListInternalInterface& parent,
        const api::session::Client& api,
        const UnitListRowID& rowID,
        const UnitListSortKey& sortKey,
        CustomData& custom) noexcept;
    UnitListItem() = delete;
    UnitListItem(const UnitListItem&) = delete;
    UnitListItem(UnitListItem&&) = delete;
    auto operator=(const UnitListItem&) -> UnitListItem& = delete;
    auto operator=(UnitListItem&&) -> UnitListItem& = delete;

    ~UnitListItem() final = default;

private:
    const UnitListSortKey name_;

    auto qt_data(const int column, const int role, QVariant& out) const noexcept
        -> void final;

    auto reindex(const UnitListSortKey&, CustomData&) noexcept -> bool final
    {
        return false;
    }
};
}  // namespace opentxs::ui::implementation

template class opentxs::SharedPimpl<opentxs::ui::UnitListItem>;
