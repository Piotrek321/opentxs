// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/serialization/protobuf/verify/ContactEvent.hpp"  // IWYU pragma: associated

#include <ContactEvent.pb.h>
#include <RPCEnums.pb.h>

#include "internal/serialization/protobuf/Basic.hpp"
#include "internal/serialization/protobuf/verify/AccountEvent.hpp"  // IWYU pragma: keep
#include "internal/serialization/protobuf/verify/VerifyRPC.hpp"
#include "serialization/protobuf/verify/Check.hpp"

namespace opentxs::proto
{
auto CheckProto_1(const ContactEvent& input, const bool silent) -> bool
{
    CHECK_IDENTIFIER(id);

    switch (input.type()) {
        case CONTACTEVENT_INCOMINGMESSAGE:
        case CONTACTEVENT_OUTGOINGMESSAGE: {
            CHECK_EXISTS_STRING(message);
            CHECK_EXCLUDED(account);
            CHECK_EXCLUDED(accountevent);
        } break;
        case CONTACTEVENT_INCOMONGPAYMENT:
        case CONTACTEVENT_OUTGOINGPAYMENT: {
            CHECK_EXCLUDED(timestamp);
            CHECK_EXCLUDED(message);
            CHECK_IDENTIFIER(account);
            CHECK_SUBOBJECT(accountevent, ContactEventAllowedAccountEvent());
        } break;
        case CONTACTEVENT_ERROR:
        default: {
            FAIL_2("Invalid type", input.type());
        }
    }

    return true;
}

auto CheckProto_2(const ContactEvent& input, const bool silent) -> bool
{
    return CheckProto_1(input, silent);
}

auto CheckProto_3(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(3);
}

auto CheckProto_4(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(4);
}

auto CheckProto_5(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(5);
}

auto CheckProto_6(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(6);
}

auto CheckProto_7(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(7);
}

auto CheckProto_8(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(8);
}

auto CheckProto_9(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(9);
}

auto CheckProto_10(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(10);
}

auto CheckProto_11(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(11);
}

auto CheckProto_12(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(12);
}

auto CheckProto_13(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(13);
}

auto CheckProto_14(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(14);
}

auto CheckProto_15(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(15);
}

auto CheckProto_16(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(16);
}

auto CheckProto_17(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(17);
}

auto CheckProto_18(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(18);
}

auto CheckProto_19(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(19);
}

auto CheckProto_20(const ContactEvent& input, const bool silent) -> bool
{
    UNDEFINED_VERSION(20);
}
}  // namespace opentxs::proto
