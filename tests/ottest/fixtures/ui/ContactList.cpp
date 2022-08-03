// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/ui/ContactList.hpp"

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <iterator>
#include <sstream>

#include "ottest/fixtures/common/Counter.hpp"
#include "ottest/fixtures/common/User.hpp"

namespace ottest
{
auto check_contact_list(
    const User& user,
    const ContactListData& expected) noexcept -> bool
{
    const auto& widget = user.api_->UI().ContactList(user.nym_id_);
    auto output{true};
    const auto& v = expected.rows_;
    auto row = widget.First();

    if (const auto valid = row->Valid(); 0 < v.size()) {
        output &= valid;

        EXPECT_TRUE(valid);

        if (false == valid) { return output; }
    } else {
        output &= (false == valid);

        EXPECT_FALSE(valid);
    }

    for (auto it{v.begin()}; it < v.end(); ++it, row = widget.Next()) {
        const auto& index = it->contact_id_index_;

        if (it->check_contact_id_) {
            const auto match =
                (row->ContactID() ==
                 user.Contact(index).asBase58(user.api_->Crypto()));

            output &= match;

            EXPECT_EQ(
                row->ContactID(),
                user.Contact(index).asBase58(user.api_->Crypto()));
        } else {
            const auto set = user.SetContact(index, row->ContactID());
            const auto exists = (false == user.Contact(index).empty());

            output &= set;
            output &= exists;

            EXPECT_TRUE(set);
            EXPECT_TRUE(exists);
        }

        output &= (row->DisplayName() == it->name_);
        output &= (row->ImageURI() == it->image_);
        output &= (row->Section() == it->section_);

        EXPECT_EQ(row->DisplayName(), it->name_);
        EXPECT_EQ(row->ImageURI(), it->image_);
        EXPECT_EQ(row->Section(), it->section_);

        const auto lastVector = std::next(it) == v.end();
        const auto lastRow = row->Last();
        output &= (lastVector == lastRow);

        if (lastVector) {
            EXPECT_TRUE(lastRow);
        } else {
            EXPECT_FALSE(lastRow);

            if (lastRow) { return output; }
        }
    }

    return output;
}

auto check_messagable_list(
    const User& user,
    const ContactListData& expected) noexcept -> bool
{
    const auto& widget = user.api_->UI().MessagableList(user.nym_id_);
    auto output{true};
    const auto& v = expected.rows_;
    auto row = widget.First();

    if (const auto valid = row->Valid(); 0 < v.size()) {
        output &= valid;

        EXPECT_TRUE(valid);

        if (false == valid) { return output; }
    } else {
        output &= (false == valid);

        EXPECT_FALSE(valid);
    }

    for (auto it{v.begin()}; it < v.end(); ++it, row = widget.Next()) {
        const auto& index = it->contact_id_index_;

        if (it->check_contact_id_) {
            const auto match =
                (row->ContactID() ==
                 user.Contact(index).asBase58(user.api_->Crypto()));

            output &= match;

            EXPECT_EQ(
                row->ContactID(),
                user.Contact(index).asBase58(user.api_->Crypto()));
        } else {
            const auto set = user.SetContact(index, row->ContactID());
            const auto exists = (false == user.Contact(index).empty());

            output &= set;
            output &= exists;

            EXPECT_TRUE(set);
            EXPECT_TRUE(exists);
        }

        output &= (row->DisplayName() == it->name_);
        output &= (row->ImageURI() == it->image_);
        output &= (row->Section() == it->section_);

        EXPECT_EQ(row->DisplayName(), it->name_);
        EXPECT_EQ(row->ImageURI(), it->image_);
        EXPECT_EQ(row->Section(), it->section_);

        const auto lastVector = std::next(it) == v.end();
        const auto lastRow = row->Last();
        output &= (lastVector == lastRow);

        if (lastVector) {
            EXPECT_TRUE(lastRow);
        } else {
            EXPECT_FALSE(lastRow);

            if (lastRow) { return output; }
        }
    }

    return output;
}

auto contact_list_add_contact(
    const User& user,
    const ot::UnallocatedCString& label,
    const ot::UnallocatedCString& paymentCode,
    const ot::UnallocatedCString& nymID) noexcept -> ot::UnallocatedCString
{
    const auto& widget = user.api_->UI().ContactList(user.nym_id_);

    return widget.AddContact(label, paymentCode, nymID);
}

auto init_contact_list(const User& user, Counter& counter) noexcept -> void
{
    user.api_->UI().ContactList(user.nym_id_, make_cb(counter, [&] {
                                    auto out = std::stringstream{};
                                    out << u8"contact_list_";
                                    out << user.name_lower_;

                                    return out.str();
                                }()));
    wait_for_counter(counter);
}

auto init_messagable_list(const User& user, Counter& counter) noexcept -> void
{
    user.api_->UI().MessagableList(user.nym_id_, make_cb(counter, [&] {
                                       auto out = std::stringstream{};
                                       out << u8"messagable_list_";
                                       out << user.name_lower_;

                                       return out.str();
                                   }()));
    wait_for_counter(counter);
}
}  // namespace ottest
