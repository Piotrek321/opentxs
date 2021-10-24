// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <atomic>
#include <memory>
#include <string>

#include "integration/Helpers.hpp"
#include "opentxs/OT.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/contact/Contact.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/core/crypto/PaymentCode.hpp"
#include "ui/Helpers.hpp"

namespace ottest
{
constexpr auto words_{"response seminar brave tip suit recall often sound "
                      "stick owner lottery motion"};
constexpr auto name_{"Alice"};
constexpr auto bob_{"Bob"};
constexpr auto chris_{"Chris"};
constexpr auto payment_code_1_{
    "PM8TJS2JxQ5ztXUpBBRnpTbcUXbUHy2T1abfrb3KkAAtMEGNbey4oumH7Hc578WgQJhPjBxteQ"
    "5GHHToTYHE3A1w6p7tU6KSoFmWBVbFGjKPisZDbP97"};
constexpr auto payment_code_2_{
    "PM8TJfV1DQD6VScd5AWsSax8RgK9cUREe939M1d85MwGCKJukyghX6B5E7kqcCyEYu6Tu1ZvdG"
    "8aWh6w8KGhSfjgL8fBKuZS6aUjhV9xLV1R16CcgWhw"};

Counter counter_{1, 0};

class Test_ContactList : public ::testing::Test
{
public:
    static const User alice_;

    const ot::api::client::Manager& api_;
    ot::OTPasswordPrompt reason_;
    const ot::OTPaymentCode bob_payment_code_;
    const ot::OTPaymentCode chris_payment_code_;

    Test_ContactList()
        : api_(ot::Context().StartClient(0))
        , reason_(api_.Factory().PasswordPrompt(__func__))
        , bob_payment_code_(
              api_.Factory().PaymentCode(std::string{payment_code_1_}))
        , chris_payment_code_(
              api_.Factory().PaymentCode(std::string{payment_code_2_}))
    {
        const_cast<User&>(alice_).init(api_);
    }
};

const User Test_ContactList::alice_{words_, name_};

TEST_F(Test_ContactList, initialize_opentxs)
{
    init_contact_list(alice_, counter_);

    ASSERT_TRUE(bob_payment_code_->Valid());
    ASSERT_TRUE(chris_payment_code_->Valid());
}

TEST_F(Test_ContactList, initial_state)
{
    ASSERT_TRUE(wait_for_counter(counter_));

    const auto expected = ContactListData{{
        {true, alice_.name_, alice_.name_, "ME", ""},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_contact_list(alice_, expected));
    EXPECT_TRUE(check_contact_list_qt(alice_, expected));
}

TEST_F(Test_ContactList, add_chris)
{
    counter_.expected_ += 1;
    const auto chris = api_.Contacts().NewContact(
        chris_, chris_payment_code_->ID(), chris_payment_code_);

    ASSERT_TRUE(chris);

    alice_.SetContact(chris_, chris->ID());
}

TEST_F(Test_ContactList, add_chris_state)
{
    ASSERT_TRUE(wait_for_counter(counter_));

    const auto expected = ContactListData{{
        {true, alice_.name_, alice_.name_, "ME", ""},
        {true, chris_, chris_, "C", ""},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_contact_list(alice_, expected));
    EXPECT_TRUE(check_contact_list_qt(alice_, expected));
}

TEST_F(Test_ContactList, add_bob)
{
    counter_.expected_ += 1;
    const auto bob = api_.Contacts().NewContact(
        bob_, bob_payment_code_->ID(), bob_payment_code_);

    ASSERT_TRUE(bob);

    alice_.SetContact(bob_, bob->ID());
}

TEST_F(Test_ContactList, add_bob_state)
{
    ASSERT_TRUE(wait_for_counter(counter_));

    const auto expected = ContactListData{{
        {true, alice_.name_, alice_.name_, "ME", ""},
        {true, bob_, bob_, "B", ""},
        {true, chris_, chris_, "C", ""},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_contact_list(alice_, expected));
    EXPECT_TRUE(check_contact_list_qt(alice_, expected));
}

TEST_F(Test_ContactList, shutdown)
{
    EXPECT_EQ(counter_.expected_, counter_.updated_);
}
}  // namespace ottest
