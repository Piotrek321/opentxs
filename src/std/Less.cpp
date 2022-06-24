// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated

#include <memory>
#include <utility>

#include "internal/interface/ui/UI.hpp"
#include "internal/otx/client/Client.hpp"
#include "internal/otx/client/OTPayment.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Hash.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Header.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/FixedByteArray.hpp"
#include "opentxs/core/Secret.hpp"
#include "opentxs/core/contract/peer/PeerReply.hpp"
#include "opentxs/core/contract/peer/PeerRequest.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Notary.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/crypto/Seed.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Pimpl.hpp"  // IWYU pragma: keep

namespace std
{
auto less<opentxs::blockchain::block::Hash>::operator()(
    const opentxs::blockchain::block::Hash& lhs,
    const opentxs::blockchain::block::Hash& rhs) const noexcept -> bool
{
    return lhs < rhs;
}

auto less<opentxs::blockchain::block::Position>::operator()(
    const opentxs::blockchain::block::Position& lhs,
    const opentxs::blockchain::block::Position& rhs) const noexcept -> bool
{
    return lhs < rhs;
}

auto less<opentxs::blockchain::cfilter::Hash>::operator()(
    const opentxs::blockchain::cfilter::Hash& lhs,
    const opentxs::blockchain::cfilter::Hash& rhs) const noexcept -> bool
{
    return lhs < rhs;
}

auto less<opentxs::blockchain::cfilter::Header>::operator()(
    const opentxs::blockchain::cfilter::Header& lhs,
    const opentxs::blockchain::cfilter::Header& rhs) const noexcept -> bool
{
    return lhs < rhs;
}

auto less<opentxs::crypto::Seed>::operator()(
    const opentxs::crypto::Seed& lhs,
    const opentxs::crypto::Seed& rhs) const noexcept -> bool
{
    return lhs < rhs;
}

auto less<opentxs::otx::client::MessageTask>::operator()(
    const opentxs::otx::client::MessageTask& lhs,
    const opentxs::otx::client::MessageTask& rhs) const noexcept -> bool
{
    const auto& [lID, lMessage, lFunction] = lhs;
    const auto& [rID, rMessage, rFunction] = rhs;

    if (lID->str() < rID->str()) { return true; }

    if (rID->str() < lID->str()) { return false; }

    if (lMessage < rMessage) { return true; }

    if (rMessage < lMessage) { return false; }

    if (&lFunction < &rFunction) { return true; }

    return false;
}

auto less<opentxs::otx::client::PaymentTask>::operator()(
    const opentxs::otx::client::PaymentTask& lhs,
    const opentxs::otx::client::PaymentTask& rhs) const noexcept -> bool
{
    const auto& [lID, lPayment] = lhs;
    const auto& [rID, rPayment] = rhs;

    if (lID->str() < rID->str()) { return true; }

    if (rID->str() < lID->str()) { return false; }

    auto lPaymentID = opentxs::Identifier::Factory();
    auto rPaymentID = opentxs::Identifier::Factory();

    lPayment->GetIdentifier(lPaymentID);
    rPayment->GetIdentifier(rPaymentID);

    if (lPaymentID->str() < rPaymentID->str()) { return true; }

    return false;
}

auto less<opentxs::otx::client::PeerReplyTask>::operator()(
    const opentxs::otx::client::PeerReplyTask& lhs,
    const opentxs::otx::client::PeerReplyTask& rhs) const noexcept -> bool
{
    const auto& [lNym, lReply, lRequest] = lhs;
    const auto& [rNym, rReply, rRequest] = rhs;

    if (lNym->str() < rNym->str()) { return true; }

    if (rNym->str() < lNym->str()) { return false; }

    if (lReply->ID()->str() < rReply->ID()->str()) { return true; }

    if (rReply->ID()->str() < lReply->ID()->str()) { return false; }

    if (lRequest->ID()->str() < rRequest->ID()->str()) { return true; }

    return false;
}

auto less<opentxs::otx::client::PeerRequestTask>::operator()(
    const opentxs::otx::client::PeerRequestTask& lhs,
    const opentxs::otx::client::PeerRequestTask& rhs) const noexcept -> bool
{
    const auto& [lID, lRequest] = lhs;
    const auto& [rID, rRequest] = rhs;

    if (lID->str() < rID->str()) { return true; }

    if (rID->str() < lID->str()) { return false; }

    if (lRequest->ID()->str() < rRequest->ID()->str()) { return true; }

    return false;
}

auto less<opentxs::ui::implementation::ActivityThreadRowID>::operator()(
    const opentxs::ui::implementation::ActivityThreadRowID& lhs,
    const opentxs::ui::implementation::ActivityThreadRowID& rhs) const -> bool
{
    const auto& [lID, lBox, lAccount] = lhs;
    const auto& [rID, rBox, rAccount] = rhs;

    if (lID->str() < rID->str()) { return true; }

    if (rID->str() < lID->str()) { return false; }

    if (lBox < rBox) { return true; }

    if (rBox < lBox) { return false; }

    if (lAccount->str() < rAccount->str()) { return true; }

    return false;
}

auto less<opentxs::ui::implementation::BlockchainSelectionSortKey>::operator()(
    const opentxs::ui::implementation::BlockchainSelectionSortKey& lhs,
    const opentxs::ui::implementation::BlockchainSelectionSortKey& rhs) const
    -> bool
{
    const auto& [lName, lTestnet] = lhs;
    const auto& [rName, rTestnet] = rhs;

    if ((!lTestnet) && (rTestnet)) { return true; }

    if (lTestnet && (!rTestnet)) { return false; }

    if (lName < rName) { return true; }

    return false;
}

auto less<opentxs::ui::implementation::ContactListSortKey>::operator()(
    const opentxs::ui::implementation::ContactListSortKey& lhs,
    const opentxs::ui::implementation::ContactListSortKey& rhs) const -> bool
{
    const auto& [lSelf, lText] = lhs;
    const auto& [rSelf, rText] = rhs;

    if (lSelf && (!rSelf)) { return true; }

    if (rSelf && (!lSelf)) { return false; }

    if (lText < rText) { return true; }

    return false;
}

auto less<opentxs::ByteArray>::operator()(
    const opentxs::ByteArray& lhs,
    const opentxs::ByteArray& rhs) const -> bool
{
    return lhs < rhs;
}

auto less<opentxs::FixedByteArray<32>>::operator()(
    const opentxs::FixedByteArray<32>& lhs,
    const opentxs::FixedByteArray<32>& rhs) const -> bool
{
    return lhs < rhs;
}

auto less<opentxs::OTIdentifier>::operator()(
    const opentxs::OTIdentifier& lhs,
    const opentxs::OTIdentifier& rhs) const -> bool
{
    return lhs.get() < rhs.get();
}

auto less<opentxs::OTNotaryID>::operator()(
    const opentxs::OTNotaryID& lhs,
    const opentxs::OTNotaryID& rhs) const -> bool
{
    return lhs.get() < rhs.get();
}

auto less<opentxs::OTNymID>::operator()(
    const opentxs::OTNymID& lhs,
    const opentxs::OTNymID& rhs) const -> bool
{
    return lhs.get() < rhs.get();
}

auto less<opentxs::OTPeerReply>::operator()(
    const opentxs::OTPeerReply& lhs,
    const opentxs::OTPeerReply& rhs) const -> bool
{
    return lhs < rhs;
}

auto less<opentxs::OTPeerRequest>::operator()(
    const opentxs::OTPeerRequest& lhs,
    const opentxs::OTPeerRequest& rhs) const -> bool
{
    return lhs < rhs;
}

auto less<opentxs::OTSecret>::operator()(
    const opentxs::OTSecret& lhs,
    const opentxs::OTSecret& rhs) const -> bool
{
    return lhs.get() < rhs.get();
}

auto less<opentxs::OTUnitID>::operator()(
    const opentxs::OTUnitID& lhs,
    const opentxs::OTUnitID& rhs) const -> bool
{
    return lhs.get() < rhs.get();
}

auto less<opentxs::OT_DownloadNymboxType>::operator()(
    const opentxs::OT_DownloadNymboxType& lhs,
    const opentxs::OT_DownloadNymboxType& rhs) const noexcept -> bool
{
    return lhs < rhs;
}

auto less<opentxs::OT_GetTransactionNumbersType>::operator()(
    const opentxs::OT_GetTransactionNumbersType& lhs,
    const opentxs::OT_GetTransactionNumbersType& rhs) const noexcept -> bool
{
    return lhs < rhs;
}
}  // namespace std
