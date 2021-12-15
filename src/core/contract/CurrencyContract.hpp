// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <string>

#include "Proto.hpp"
#include "core/contract/UnitDefinition.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/contact/ClaimType.hpp"
#include "opentxs/contact/Types.hpp"
#include "opentxs/core/Types.hpp"
#include "opentxs/core/UnitType.hpp"
#include "opentxs/core/contract/CurrencyContract.hpp"
#include "opentxs/core/contract/UnitType.hpp"
#include "opentxs/util/Numbers.hpp"
#include "serialization/protobuf/UnitDefinition.pb.h"

namespace opentxs
{
namespace api
{
class Session;
}  // namespace api

class PasswordPrompt;
}  // namespace opentxs

namespace opentxs::contract::unit::implementation
{
class Currency final : public unit::Currency,
                       public contract::implementation::Unit
{
public:
    auto Type() const -> contract::UnitType final
    {
        return contract::UnitType::Currency;
    }

    Currency(
        const api::Session& api,
        const Nym_p& nym,
        const std::string& shortname,
        const std::string& terms,
        const core::UnitType unitOfAccount,
        const VersionNumber version,
        const display::Definition& displayDefinition,
        const Amount& redemptionIncrement);
    Currency(
        const api::Session& api,
        const Nym_p& nym,
        const proto::UnitDefinition serialized);

    ~Currency() final = default;

private:
    auto clone() const noexcept -> Currency* final
    {
        return new Currency(*this);
    }
    auto IDVersion(const Lock& lock) const -> proto::UnitDefinition final;

    Currency(const Currency&);
    Currency(Currency&&) = delete;
    auto operator=(const Currency&) -> Currency& = delete;
    auto operator=(Currency&&) -> Currency& = delete;
};
}  // namespace opentxs::contract::unit::implementation
