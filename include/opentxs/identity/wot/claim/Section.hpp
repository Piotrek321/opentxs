// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// IWYU pragma: no_include "opentxs/identity/wot/claim/SectionType.hpp"

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstddef>
#include <iosfwd>
#include <memory>

#include "opentxs/identity/wot/claim/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
class Session;
}  // namespace api

namespace identifier
{
class Generic;
}  // namespace identifier

namespace identity
{
namespace wot
{
namespace claim
{
class Group;
class Item;
}  // namespace claim
}  // namespace wot
}  // namespace identity

namespace proto
{
class ContactData;
class ContactSection;
}  // namespace proto
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::identity::wot::claim
{
class OPENTXS_EXPORT Section
{
public:
    using GroupMap =
        UnallocatedMap<claim::ClaimType, std::shared_ptr<claim::Group>>;

    auto operator+(const Section& rhs) const -> Section;

    auto AddItem(const std::shared_ptr<Item>& item) const -> Section;
    auto begin() const -> GroupMap::const_iterator;
    auto Claim(const identifier::Generic& item) const -> std::shared_ptr<Item>;
    auto Delete(const identifier::Generic& id) const -> Section;
    auto end() const -> GroupMap::const_iterator;
    auto Group(const claim::ClaimType& type) const -> std::shared_ptr<Group>;
    auto HaveClaim(const identifier::Generic& item) const -> bool;
    auto Serialize(AllocateOutput destination, const bool withIDs = false) const
        -> bool;
    OPENTXS_NO_EXPORT auto SerializeTo(
        proto::ContactData& data,
        const bool withIDs = false) const -> bool;
    auto Size() const -> std::size_t;
    auto Type() const -> const claim::SectionType&;
    auto Version() const -> VersionNumber;

    Section(
        const api::Session& api,
        const UnallocatedCString& nym,
        const VersionNumber version,
        const VersionNumber parentVersion,
        const claim::SectionType section,
        const GroupMap& groups);
    Section(
        const api::Session& api,
        const UnallocatedCString& nym,
        const VersionNumber version,
        const VersionNumber parentVersion,
        const claim::SectionType section,
        const std::shared_ptr<Item>& item);
    OPENTXS_NO_EXPORT Section(
        const api::Session& api,
        const UnallocatedCString& nym,
        const VersionNumber parentVersion,
        const proto::ContactSection& serialized);
    Section(
        const api::Session& api,
        const UnallocatedCString& nym,
        const VersionNumber parentVersion,
        const ReadView& serialized);
    Section() = delete;
    Section(const Section&) noexcept;
    Section(Section&&) noexcept;
    auto operator=(const Section&) -> Section& = delete;
    auto operator=(Section&&) -> Section& = delete;

    ~Section();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;
};
}  // namespace opentxs::identity::wot::claim
