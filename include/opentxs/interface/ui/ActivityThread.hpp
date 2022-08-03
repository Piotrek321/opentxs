// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include "opentxs/interface/ui/List.hpp"
#include "opentxs/otx/client/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/SharedPimpl.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace ui
{
class ActivityThread;
class ActivityThreadItem;
}  // namespace ui
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui
{
/**
  This model manages the ActivityThread between the user and one of his
  contacts. Each row in ActivityThread is a different ActivityThreadItem (chat
  message, incoming transaction, etc). This class is also a convenient way to
  grab relevant information about the current thread, such as the participants
  or the draft. Includes functionality for directly sending a payment inside the
  current thread.
 */
class OPENTXS_EXPORT ActivityThread : virtual public List
{
public:
    /// Boolean value showing whether or not this contact can be messaged.
    virtual auto CanMessage() const noexcept -> bool = 0;
    /// Returns the display name for this thread (usually the name of the
    /// Contact).
    virtual auto DisplayName() const noexcept -> UnallocatedCString = 0;
    /// returns the first row, containing a valid ActivityThreadItem or an empty
    /// smart pointer (if list is empty).
    virtual auto First() const noexcept
        -> opentxs::SharedPimpl<opentxs::ui::ActivityThreadItem> = 0;
    /// Returns the current draft (contains the draft of the newest outgoing
    /// message, not yet sent).
    virtual auto GetDraft() const noexcept -> UnallocatedCString = 0;
    /// returns the next row, containing a valid ActivityThreadItem or an empty
    /// smart pointer (if at end of list).
    virtual auto Next() const noexcept
        -> opentxs::SharedPimpl<opentxs::ui::ActivityThreadItem> = 0;
    /// Returns a string containing the participants in this thread.
    virtual auto Participants() const noexcept -> UnallocatedCString = 0;
    ///@{
    /**
       @name Pay
       @param amount The amount being sent.
       @param sourceAccount The account where the funds are drawn from.
       @param memo Optional memo for the outgoing payment.
       @param type Type of payment being sent.
       @return Bool indicating whether payment was successfully sent.
    */
    virtual auto Pay(
        const UnallocatedCString& amount,
        const identifier::Generic& sourceAccount,
        const UnallocatedCString& memo = "",
        const otx::client::PaymentType type =
            otx::client::PaymentType::Cheque) const noexcept -> bool = 0;
    virtual auto Pay(
        const Amount amount,
        const identifier::Generic& sourceAccount,
        const UnallocatedCString& memo = "",
        const otx::client::PaymentType type =
            otx::client::PaymentType::Cheque) const noexcept -> bool = 0;
    ///@}
    /// Returns the payment code for the contact relevant to this thread.
    virtual auto PaymentCode(const UnitType currency) const noexcept
        -> UnallocatedCString = 0;
    /// Sends the current draft.
    virtual auto SendDraft() const noexcept -> bool = 0;
    /// Whenever the user makes edits to his newest unsent message, save the
    /// latest version here.
    virtual auto SetDraft(const UnallocatedCString& draft) const noexcept
        -> bool = 0;
    /// Returns the ID for this thread.
    virtual auto ThreadID() const noexcept -> UnallocatedCString = 0;

    ActivityThread(const ActivityThread&) = delete;
    ActivityThread(ActivityThread&&) = delete;
    auto operator=(const ActivityThread&) -> ActivityThread& = delete;
    auto operator=(ActivityThread&&) -> ActivityThread& = delete;

    ~ActivityThread() override = default;

protected:
    ActivityThread() noexcept = default;
};
}  // namespace opentxs::ui
