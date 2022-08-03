// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>

#include "opentxs/identity/credential/Base.hpp"
#include "opentxs/identity/wot/claim/Types.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
class Session;
}  // namespace api

namespace proto
{
class Claim;
class ContactItem;
}  // namespace proto
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::identity::credential
{
class OPENTXS_EXPORT Contact : virtual public Base
{
public:
    OPENTXS_NO_EXPORT static auto ClaimID(
        const api::Session& api,
        const UnallocatedCString& nymid,
        const std::uint32_t section,
        const proto::ContactItem& item) -> UnallocatedCString;
    static auto ClaimID(
        const api::Session& api,
        const UnallocatedCString& nymid,
        const wot::claim::SectionType section,
        const wot::claim::ClaimType type,
        const std::int64_t start,
        const std::int64_t end,
        const UnallocatedCString& value,
        const UnallocatedCString& subtype) -> UnallocatedCString;
    OPENTXS_NO_EXPORT static auto ClaimID(
        const api::Session& api,
        const proto::Claim& preimage) -> identifier::Generic;
    OPENTXS_NO_EXPORT static auto asClaim(
        const api::Session& api,
        const String& nymid,
        const std::uint32_t section,
        const proto::ContactItem& item) -> Claim;

    Contact(const Contact&) = delete;
    Contact(Contact&&) = delete;
    auto operator=(const Contact&) -> Contact& = delete;
    auto operator=(Contact&&) -> Contact& = delete;

    ~Contact() override = default;

protected:
    Contact() noexcept = default;  // TODO Signable
};
}  // namespace opentxs::identity::credential
