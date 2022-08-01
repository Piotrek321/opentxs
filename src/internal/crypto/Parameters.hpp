// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace proto
{
class ContactData;
class VerificationSet;
}  // namespace proto

class ByteArray;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::crypto::internal
{
class Parameters
{
public:
    virtual auto operator<(const Parameters& rhs) const noexcept -> bool = 0;
    virtual auto operator==(const Parameters& rhs) const noexcept -> bool = 0;

    virtual auto GetContactData(proto::ContactData& serialized) const noexcept
        -> bool = 0;
    virtual auto GetVerificationSet(
        proto::VerificationSet& serialized) const noexcept -> bool = 0;
    virtual auto Hash() const noexcept -> ByteArray = 0;

    virtual auto SetContactData(const proto::ContactData& contactData) noexcept
        -> void = 0;
    virtual auto SetVerificationSet(
        const proto::VerificationSet& verificationSet) noexcept -> void = 0;

    virtual ~Parameters() = default;
};
}  // namespace opentxs::crypto::internal
