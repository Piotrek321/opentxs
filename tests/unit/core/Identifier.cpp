// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <Identifier.pb.h>
#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>

#include "internal/api/FactoryAPI.hpp"
#include "internal/util/P0330.hpp"
#include "ottest/fixtures/core/Identifier.hpp"

namespace ottest
{
using namespace opentxs::literals;

TEST_F(Identifier, type)
{
    EXPECT_EQ(generic_.Type(), ot::identifier::Type::generic);
    EXPECT_EQ(notary_.Type(), ot::identifier::Type::notary);
    EXPECT_EQ(nym_.Type(), ot::identifier::Type::nym);
    EXPECT_EQ(unit_.Type(), ot::identifier::Type::unitdefinition);
}

TEST_F(Identifier, copy_constructor)
{
    generic_ = ot_.Factory().Internal().IdentifierFromRandom();
    notary_ = ot_.Factory().Internal().NotaryIDFromRandom();
    nym_ = ot_.Factory().Internal().NymIDFromRandom();
    unit_ = ot_.Factory().Internal().UnitIDFromRandom();

    {
        const auto& id = generic_;
        auto copy{id};

        EXPECT_EQ(copy, id);
        EXPECT_EQ(print(copy.Type()), print(id.Type()));
        EXPECT_EQ(print(copy.Algorithm()), print(id.Algorithm()));
        EXPECT_EQ(copy.Bytes(), id.Bytes());
    }

    {
        const auto& id = notary_;
        auto copy{id};

        EXPECT_EQ(copy, id);
        EXPECT_EQ(print(copy.Type()), print(id.Type()));
        EXPECT_EQ(print(copy.Algorithm()), print(id.Algorithm()));
        EXPECT_EQ(copy.Bytes(), id.Bytes());
    }

    {
        const auto& id = nym_;
        auto copy{id};

        EXPECT_EQ(copy, id);
        EXPECT_EQ(print(copy.Type()), print(id.Type()));
        EXPECT_EQ(print(copy.Algorithm()), print(id.Algorithm()));
        EXPECT_EQ(copy.Bytes(), id.Bytes());
    }

    {
        const auto& id = unit_;
        auto copy{id};

        EXPECT_EQ(copy, id);
        EXPECT_EQ(print(copy.Type()), print(id.Type()));
        EXPECT_EQ(print(copy.Algorithm()), print(id.Algorithm()));
        EXPECT_EQ(copy.Bytes(), id.Bytes());
    }
}

TEST_F(Identifier, generic_default_accessors)
{
    const auto& id = generic_;

    EXPECT_EQ(id.data(), nullptr);
    EXPECT_EQ(id.size(), 0_uz);
}

TEST_F(Identifier, notary_default_accessors)
{
    const auto& id = notary_;

    EXPECT_EQ(id.data(), nullptr);
    EXPECT_EQ(id.size(), 0_uz);
}

TEST_F(Identifier, nym_default_accessors)
{
    const auto& id = nym_;

    EXPECT_EQ(id.data(), nullptr);
    EXPECT_EQ(id.size(), 0_uz);
}

TEST_F(Identifier, unit_default_accessors)
{
    const auto& id = unit_;

    EXPECT_EQ(id.data(), nullptr);
    EXPECT_EQ(id.size(), 0_uz);
}

TEST_F(Identifier, generic_serialize_base58_empty)
{
    const auto& id = generic_;
    const auto base58 = id.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().IdentifierFromBase58(base58);

    EXPECT_EQ(id, recovered);
    EXPECT_EQ(id.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(Identifier, notary_serialize_base58_empty)
{
    const auto& id = generic_;
    const auto base58 = notary_.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().NotaryIDFromBase58(base58);

    EXPECT_EQ(notary_, recovered);
    EXPECT_EQ(id.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(Identifier, nym_serialize_base58_empty)
{
    const auto& id = nym_;
    const auto base58 = id.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().NymIDFromBase58(base58);

    EXPECT_EQ(id, recovered);
    EXPECT_EQ(id.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(Identifier, unit_serialize_base58_empty)
{
    const auto& id = unit_;
    const auto base58 = id.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().UnitIDFromBase58(base58);

    EXPECT_EQ(id, recovered);
    EXPECT_EQ(id.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(GenericID, generic_serialize_base58_non_empty)
{
    const auto base58 = id_.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().IdentifierFromBase58(base58);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(NotaryID, notary_serialize_base58_non_empty)
{
    const auto base58 = id_.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().NotaryIDFromBase58(base58);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(NymID, nym_serialize_base58_non_empty)
{
    const auto base58 = id_.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().NymIDFromBase58(base58);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(UnitID, unit_serialize_base58_non_empty)
{
    const auto base58 = id_.asBase58(ot_.Crypto());
    const auto recovered = ot_.Factory().UnitIDFromBase58(base58);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(GenericID, generic_serialize_protobuf_non_empty)
{
    auto proto = ot::proto::Identifier{};

    EXPECT_TRUE(id_.Serialize(proto));

    const auto recovered = ot_.Factory().Internal().Identifier(proto);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(NotaryID, notary_serialize_protobuf_non_empty)
{
    auto proto = ot::proto::Identifier{};

    EXPECT_TRUE(id_.Serialize(proto));

    const auto recovered = ot_.Factory().Internal().NotaryID(proto);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(NymID, nym_serialize_protobuf_non_empty)
{
    auto proto = ot::proto::Identifier{};

    EXPECT_TRUE(id_.Serialize(proto));

    const auto recovered = ot_.Factory().Internal().NymID(proto);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}

TEST_F(UnitID, unit_serialize_protobuf_non_empty)
{
    auto proto = ot::proto::Identifier{};

    EXPECT_TRUE(id_.Serialize(proto));

    const auto recovered = ot_.Factory().Internal().UnitID(proto);

    EXPECT_EQ(id_, recovered);
    EXPECT_EQ(id_.asBase58(ot_.Crypto()), recovered.asBase58(ot_.Crypto()));
}
}  // namespace ottest
