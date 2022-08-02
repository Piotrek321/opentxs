// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/serialization/protobuf/verify/NymIDSource.hpp"  // IWYU pragma: associated

#include <AsymmetricKey.pb.h>  // IWYU pragma: keep
#include <Enums.pb.h>
#include <NymIDSource.pb.h>
#include <PaymentCode.pb.h>  // IWYU pragma: keep
#include <stdexcept>
#include <utility>

#include "internal/serialization/protobuf/Basic.hpp"
#include "internal/serialization/protobuf/Check.hpp"
#include "internal/serialization/protobuf/verify/AsymmetricKey.hpp"  // IWYU pragma: keep
#include "internal/serialization/protobuf/verify/PaymentCode.hpp"  // IWYU pragma: keep
#include "internal/serialization/protobuf/verify/VerifyCredentials.hpp"
#include "serialization/protobuf/verify/Check.hpp"

namespace opentxs::proto
{

auto CheckProto_1(const NymIDSource& input, const bool silent) -> bool
{
    if (!input.has_type()) { FAIL_1("missing type"); }

    bool validSourcePubkey{false};
    bool validPaymentCode{false};

    switch (input.type()) {
        case SOURCETYPE_PUBKEY: {
            if (!input.has_key()) { FAIL_1("missing key"); }

            if (input.has_paymentcode()) {
                FAIL_1("pubkey source includes payment code");
            }

            try {
                validSourcePubkey = Check(
                    input.key(),
                    NymIDSourceAllowedAsymmetricKey().at(input.version()).first,
                    NymIDSourceAllowedAsymmetricKey()
                        .at(input.version())
                        .second,
                    silent,
                    CREDTYPE_LEGACY,
                    KEYMODE_PUBLIC,
                    KEYROLE_SIGN);

                if (!validSourcePubkey) { FAIL_1("invalid public key"); }
            } catch (const std::out_of_range&) {
                FAIL_2(
                    "allowed asymmetric key version not defined for version",
                    input.version());
            }

        } break;
        case SOURCETYPE_BIP47: {
            if (!input.has_paymentcode()) { FAIL_1("missing payment code"); }

            if (input.has_key()) { FAIL_1("bip47 source includes public key"); }

            try {
                validPaymentCode = Check(
                    input.paymentcode(),
                    NymIDSourceAllowedPaymentCode().at(input.version()).first,
                    NymIDSourceAllowedPaymentCode().at(input.version()).second,
                    silent);

                if (!validPaymentCode) { FAIL_1("invalid payment code"); }
            } catch (const std::out_of_range&) {
                FAIL_2(
                    "allowed payment code version not defined for version",
                    input.version());
            }

        } break;
        case SOURCETYPE_ERROR:
        default:
            FAIL_2("incorrect or unknown type", input.type());
    }

    return true;
}

auto CheckProto_2(const NymIDSource& input, const bool silent) -> bool
{
    return CheckProto_1(input, silent);
}

auto CheckProto_3(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(3);
}

auto CheckProto_4(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(4);
}

auto CheckProto_5(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(5);
}

auto CheckProto_6(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(6);
}

auto CheckProto_7(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(7);
}

auto CheckProto_8(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(8);
}

auto CheckProto_9(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(9);
}

auto CheckProto_10(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(10);
}

auto CheckProto_11(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(11);
}

auto CheckProto_12(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(12);
}

auto CheckProto_13(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(13);
}

auto CheckProto_14(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(14);
}

auto CheckProto_15(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(15);
}

auto CheckProto_16(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(16);
}

auto CheckProto_17(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(17);
}

auto CheckProto_18(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(18);
}

auto CheckProto_19(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(19);
}

auto CheckProto_20(const NymIDSource& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(20);
}
}  // namespace opentxs::proto
