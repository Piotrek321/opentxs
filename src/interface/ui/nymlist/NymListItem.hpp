// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cs_ordered_guarded.h>
#include <iosfwd>
#include <mutex>
#include <shared_mutex>

#include "1_Internal.hpp"
#include "interface/ui/base/Row.hpp"
#include "internal/interface/ui/UI.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/interface/ui/NymListItem.hpp"
#include "opentxs/util/Container.hpp"
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
class NymListItem;
}  // namespace ui
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui::implementation
{
using NymListItemRow =
    Row<NymListRowInternal, NymListInternalInterface, NymListRowID>;

class NymListItem : public NymListItemRow
{
public:
    const api::session::Client& api_;

    auto NymID() const noexcept -> UnallocatedCString final;
    auto Name() const noexcept -> UnallocatedCString final;

    NymListItem(
        const NymListInternalInterface& parent,
        const api::session::Client& api,
        const NymListRowID& rowID,
        const NymListSortKey& key) noexcept;
    NymListItem() = delete;
    NymListItem(const NymListItem&) = delete;
    NymListItem(NymListItem&&) = delete;
    auto operator=(const NymListItem&) -> NymListItem& = delete;
    auto operator=(NymListItem&&) -> NymListItem& = delete;

    ~NymListItem() override = default;

protected:
    auto qt_data(const int column, const int role, QVariant& out) const noexcept
        -> void override;
    auto reindex(const NymListSortKey&, CustomData&) noexcept -> bool final;

private:
    libguarded::ordered_guarded<NymListSortKey, std::shared_mutex> name_;
};
}  // namespace opentxs::ui::implementation
