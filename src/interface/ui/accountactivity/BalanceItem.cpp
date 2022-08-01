// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "interface/ui/accountactivity/BalanceItem.hpp"  // IWYU pragma: associated

#include <chrono>
#include <memory>

#include "Proto.hpp"
#if OT_BLOCKCHAIN
#include "interface/ui/accountactivity/BlockchainBalanceItem.hpp"
#endif  // OT_BLOCKCHAIN
#include "interface/ui/accountactivity/ChequeBalanceItem.hpp"
#include "interface/ui/accountactivity/TransferBalanceItem.hpp"
#include "interface/ui/base/Widget.hpp"
#include "internal/api/session/Types.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/api/session/Contacts.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#if OT_BLOCKCHAIN
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#endif  // OT_BLOCKCHAIN
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/contract/Unit.hpp"
#include "opentxs/core/display/Definition.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/otx/client/PaymentWorkflowType.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "serialization/protobuf/PaymentWorkflow.pb.h"

namespace opentxs::factory
{
auto BalanceItem(
    const ui::implementation::AccountActivityInternalInterface& parent,
    const api::session::Client& api,
    const ui::implementation::AccountActivityRowID& rowID,
    const ui::implementation::AccountActivitySortKey& sortKey,
    ui::implementation::CustomData& custom,
    const identifier::Nym& nymID,
    const Identifier& accountID) noexcept
    -> std::shared_ptr<ui::implementation::AccountActivityRowInternal>
{
#if OT_BLOCKCHAIN
    if (2 < custom.size()) {
        using Transaction = opentxs::blockchain::bitcoin::block::Transaction;

        auto pTx =
            ui::implementation::extract_custom_ptr<Transaction>(custom, 2);

        OT_ASSERT(pTx);

        const auto& tx = *pTx;

        return std::make_shared<ui::implementation::BlockchainBalanceItem>(
            parent,
            api,
            rowID,
            sortKey,
            custom,
            nymID,
            accountID,
            ui::implementation::extract_custom<blockchain::Type>(custom, 3),
            ui::implementation::extract_custom<ByteArray>(custom, 5),
            tx.NetBalanceChange(nymID),
            tx.Memo(),
            ui::implementation::extract_custom<UnallocatedCString>(custom, 4));
    }
#endif  // OT_BLOCKCHAIN

    const auto type =
        ui::implementation::BalanceItem::recover_workflow(custom).type();

    switch (translate(type)) {
        case otx::client::PaymentWorkflowType::OutgoingCheque:
        case otx::client::PaymentWorkflowType::IncomingCheque:
        case otx::client::PaymentWorkflowType::OutgoingInvoice:
        case otx::client::PaymentWorkflowType::IncomingInvoice: {
            return std::make_shared<ui::implementation::ChequeBalanceItem>(
                parent, api, rowID, sortKey, custom, nymID, accountID);
        }
        case otx::client::PaymentWorkflowType::OutgoingTransfer:
        case otx::client::PaymentWorkflowType::IncomingTransfer:
        case otx::client::PaymentWorkflowType::InternalTransfer: {
            return std::make_shared<ui::implementation::TransferBalanceItem>(
                parent, api, rowID, sortKey, custom, nymID, accountID);
        }
        case otx::client::PaymentWorkflowType::Error:
        default: {
            LogError()("opentxs::factory::")(__func__)(
                "Unhandled workflow type (")(type)(")")
                .Flush();
        }
    }

    return nullptr;
}
}  // namespace opentxs::factory

namespace opentxs::ui::implementation
{
BalanceItem::BalanceItem(
    const AccountActivityInternalInterface& parent,
    const api::session::Client& api,
    const AccountActivityRowID& rowID,
    const AccountActivitySortKey& sortKey,
    CustomData& custom,
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const UnallocatedCString& text) noexcept
    : BalanceItemRow(parent, api, rowID, true)
    , nym_id_(nymID)
    , workflow_(recover_workflow(custom).id())
    , type_(extract_type(recover_workflow(custom)))
    , text_(text)
    , time_(sortKey)
    , account_id_(Identifier::Factory(accountID))
    , contacts_(extract_contacts(api_, recover_workflow(custom)))
{
}

auto BalanceItem::DisplayAmount() const noexcept -> UnallocatedCString
{
    sLock lock(shared_lock_);
    const auto& amount = effective_amount();
    const auto& definition =
        display::GetDefinition(parent_.Contract().UnitOfAccount());
    UnallocatedCString output = definition.Format(amount);

    if (0 < output.size()) { return output; }

    amount.Serialize(writer(output));
    return output;
}

auto BalanceItem::extract_contacts(
    const api::session::Client& api,
    const proto::PaymentWorkflow& workflow) noexcept
    -> UnallocatedVector<UnallocatedCString>
{
    UnallocatedVector<UnallocatedCString> output{};

    for (const auto& party : workflow.party()) {
        const auto contactID =
            api.Contacts().NymToContact(identifier::Nym::Factory(party));
        output.emplace_back(contactID->str());
    }

    return output;
}

auto BalanceItem::extract_type(const proto::PaymentWorkflow& workflow) noexcept
    -> otx::client::StorageBox
{
    switch (translate(workflow.type())) {
        case otx::client::PaymentWorkflowType::OutgoingCheque: {

            return otx::client::StorageBox::OUTGOINGCHEQUE;
        }
        case otx::client::PaymentWorkflowType::IncomingCheque: {

            return otx::client::StorageBox::INCOMINGCHEQUE;
        }
        case otx::client::PaymentWorkflowType::OutgoingTransfer: {

            return otx::client::StorageBox::OUTGOINGTRANSFER;
        }
        case otx::client::PaymentWorkflowType::IncomingTransfer: {

            return otx::client::StorageBox::INCOMINGTRANSFER;
        }
        case otx::client::PaymentWorkflowType::InternalTransfer: {

            return otx::client::StorageBox::INTERNALTRANSFER;
        }
        case otx::client::PaymentWorkflowType::Error:
        case otx::client::PaymentWorkflowType::OutgoingInvoice:
        case otx::client::PaymentWorkflowType::IncomingInvoice:
        default: {

            return otx::client::StorageBox::UNKNOWN;
        }
    }
}

auto BalanceItem::get_contact_name(const identifier::Nym& nymID) const noexcept
    -> UnallocatedCString
{
    if (nymID.empty()) { return {}; }

    UnallocatedCString output{nymID.str()};
    const auto contactID = api_.Contacts().ContactID(nymID);

    if (false == contactID->empty()) {
        output = api_.Contacts().ContactName(contactID);
    }

    return output;
}

auto BalanceItem::recover_workflow(CustomData& custom) noexcept
    -> const proto::PaymentWorkflow&
{
    OT_ASSERT(2 <= custom.size())

    const auto& input = custom.at(0);

    OT_ASSERT(nullptr != input)

    return *static_cast<const proto::PaymentWorkflow*>(input);
}

auto BalanceItem::reindex(
    const implementation::AccountActivitySortKey& key,
    implementation::CustomData&) noexcept -> bool
{
    eLock lock(shared_lock_);

    if (key == time_) {

        return false;
    } else {
        time_ = key;

        return true;
    }
}

auto BalanceItem::Text() const noexcept -> UnallocatedCString
{
    sLock lock(shared_lock_);

    return text_;
}

auto BalanceItem::Timestamp() const noexcept -> Time
{
    sLock lock(shared_lock_);

    return time_;
}

BalanceItem::~BalanceItem() = default;
}  // namespace opentxs::ui::implementation
