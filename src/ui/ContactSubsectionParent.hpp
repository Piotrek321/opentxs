/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#ifndef OPENTXS_UI_CONTACT_SUBSECTION_PARENT_HPP
#define OPENTXS_UI_CONTACT_SUBSECTION_PARENT_HPP

#include "opentxs/Internal.hpp"

#include <string>

namespace opentxs::ui::implementation
{
class ContactSubsectionParent
{
public:
    using ContactSubsectionIDType = OTIdentifier;
    using ContactSubsectionSortKey = int;

    virtual bool last(const ContactSubsectionIDType& id) const = 0;
    virtual void reindex_item(
        const ContactSubsectionIDType& id,
        const ContactSubsectionSortKey& newIndex) const = 0;
    virtual OTIdentifier WidgetID() const = 0;

    virtual ~ContactSubsectionParent() = default;

protected:
    ContactSubsectionParent() = default;
    ContactSubsectionParent(const ContactSubsectionParent&) = delete;
    ContactSubsectionParent(ContactSubsectionParent&&) = delete;
    ContactSubsectionParent& operator=(const ContactSubsectionParent&) = delete;
    ContactSubsectionParent& operator=(ContactSubsectionParent&&) = delete;
};
}  // opentxs::ui::implementation
#endif  // OPENTXS_UI_CONTACT_SUBSECTION_PARENT_HPP