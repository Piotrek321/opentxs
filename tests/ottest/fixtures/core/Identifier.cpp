// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/core/Identifier.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>

#include "internal/api/FactoryAPI.hpp"

namespace ottest
{
Identifier::Identifier() noexcept
    : generic_()
    , notary_()
    , nym_()
    , unit_()
{
}

GenericID::GenericID() noexcept
    : id_(generic_)
{
    id_ = ot_.Factory().Internal().IdentifierFromRandom();
}

NotaryID::NotaryID() noexcept
    : id_(notary_)
{
    id_ = ot_.Factory().Internal().NotaryIDFromRandom();
}

NymID::NymID() noexcept
    : id_(nym_)
{
    id_ = ot_.Factory().Internal().NymIDFromRandom();
}

UnitID::UnitID() noexcept
    : id_(unit_)
{
    id_ = ot_.Factory().Internal().UnitIDFromRandom();
}
}  // namespace ottest
