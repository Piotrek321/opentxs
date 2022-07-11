// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>

#include "ottest/Basic.hpp"
#include "ottest/fixtures/common/LowLevel.hpp"

namespace ottest
{
class Identifier : public LowLevel
{
protected:
    ot::identifier::Generic generic_;
    ot::identifier::Notary notary_;
    ot::identifier::Nym nym_;
    ot::identifier::UnitDefinition unit_;

    Identifier() noexcept;

    ~Identifier() override = default;
};

class GenericID : public Identifier
{
protected:
    ot::identifier::Generic id_;

    GenericID() noexcept;

    ~GenericID() override = default;
};

class NotaryID : public Identifier
{
protected:
    ot::identifier::Notary id_;

    NotaryID() noexcept;

    ~NotaryID() override = default;
};

class NymID : public Identifier
{
protected:
    ot::identifier::Nym id_;

    NymID() noexcept;

    ~NymID() override = default;
};

class UnitID : public Identifier
{
protected:
    ot::identifier::UnitDefinition id_;

    UnitID() noexcept;

    ~UnitID() override = default;
};
}  // namespace ottest
