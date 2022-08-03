// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"           // IWYU pragma: associated
#include "1_Internal.hpp"         // IWYU pragma: associated
#include "interface/rpc/RPC.hpp"  // IWYU pragma: associated

#include <chrono>
#include <future>
#include <type_traits>
#include <utility>

#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/BlockchainHandle.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/api/session/Contacts.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/OTX.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/core/PaymentCode.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Notary.hpp"  // IWYU pragma: keep
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/interface/rpc/PaymentType.hpp"
#include "opentxs/interface/rpc/ResponseCode.hpp"
#include "opentxs/interface/rpc/request/Base.hpp"
#include "opentxs/interface/rpc/request/SendPayment.hpp"
#include "opentxs/interface/rpc/response/Base.hpp"
#include "opentxs/interface/rpc/response/SendPayment.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Time.hpp"

namespace opentxs::rpc::implementation
{
auto RPC::send_payment(const request::Base& base) const noexcept
    -> std::unique_ptr<response::Base>
{
    const auto& in = base.asSendPayment();
    const auto reply = [&](const auto code) {
        return std::make_unique<response::SendPayment>(
            in, response::Base::Responses{{0, code}}, response::Base::Tasks{});
    };

    try {
        const auto& api = client_session(base);

        switch (in.PaymentType()) {
            case PaymentType::blockchain: {

                return send_payment_blockchain(api, in);
            }
            case PaymentType::cheque:
            case PaymentType::transfer:
            case PaymentType::voucher:
            case PaymentType::invoice:
            case PaymentType::blinded: {

                return send_payment_custodial(api, in);
            }
            case PaymentType::error:
            default: {

                return reply(ResponseCode::invalid);
            }
        }
    } catch (...) {

        return reply(ResponseCode::bad_session);
    }
}

auto RPC::send_payment_blockchain(
    const api::session::Client& api,
    const request::SendPayment& in) const noexcept
    -> std::unique_ptr<response::Base>
{
    auto tasks = response::Base::Tasks{};
    const auto reply = [&](const auto code) {
        return std::make_unique<response::SendPayment>(
            in, response::Base::Responses{{0, code}}, std::move(tasks));
    };

    const auto id = api.Factory().IdentifierFromBase58(in.SourceAccount());
    const auto& blockchain = api.Crypto().Blockchain();
    const auto data = blockchain.LookupAccount(id);
    const auto& [chain, owner] = data;

    if (blockchain::Type::Unknown == chain) {
        return reply(ResponseCode::account_not_found);
    }

    api.Network().Blockchain().Start(chain);

    try {
        auto future = [&] {
            const auto& [chain, owner] = data;
            const auto amount = in.Amount();
            const auto& address = in.DestinationAccount();
            const auto& memo = in.Memo();
            const auto handle = api.Network().Blockchain().GetChain(chain);
            const auto& network = handle.get();
            const auto recipient = api.Factory().PaymentCode(address);

            if (0 < recipient.Version()) {
                return network.SendToPaymentCode(
                    owner, recipient, amount, memo);
            } else {
                return network.SendToAddress(owner, address, amount, memo);
            }
        }();
        const auto [code, txid] = future.get();

        if (txid.empty()) { return reply(ResponseCode::transaction_failed); }

        tasks.emplace_back(0, txid.asHex());

        return reply(ResponseCode::txid);
    } catch (...) {

        return reply(ResponseCode::transaction_failed);
    }
}

auto RPC::send_payment_custodial(
    const api::session::Client& api,
    const request::SendPayment& in) const noexcept
    -> std::unique_ptr<response::Base>
{
    const auto contact =
        api.Factory().IdentifierFromBase58(in.RecipientContact());
    const auto source = api.Factory().IdentifierFromBase58(in.SourceAccount());
    auto tasks = response::Base::Tasks{};
    const auto reply = [&](const auto code) {
        return std::make_unique<response::SendPayment>(
            in, response::Base::Responses{{0, code}}, std::move(tasks));
    };

    if (contact.empty()) { return reply(ResponseCode::invalid); }

    if (auto c = api.Contacts().Contact(contact); false == bool(c)) {

        return reply(ResponseCode::contact_not_found);
    }

    const auto sender = api.Storage().AccountOwner(source);

    if (sender.empty()) { return reply(ResponseCode::account_owner_not_found); }

    const auto& otx = api.OTX();

    switch (otx.CanMessage(sender, contact)) {
        case otx::client::Messagability::MISSING_CONTACT:
        case otx::client::Messagability::CONTACT_LACKS_NYM:
        case otx::client::Messagability::NO_SERVER_CLAIM:
        case otx::client::Messagability::INVALID_SENDER:
        case otx::client::Messagability::MISSING_SENDER: {

            return reply(ResponseCode::no_path_to_recipient);
        }
        case otx::client::Messagability::MISSING_RECIPIENT:
        case otx::client::Messagability::UNREGISTERED: {

            return reply(ResponseCode::retry);
        }
        case otx::client::Messagability::READY:
        default: {
        }
    }

    switch (in.PaymentType()) {
        case PaymentType::cheque: {
            auto [taskID, future] = otx.SendCheque(
                sender,
                source,
                contact,
                in.Amount(),
                in.Memo(),
                Clock::now(),
                Clock::now() + std::chrono::hours(OT_CHEQUE_HOURS));

            if (0 == taskID) { return reply(ResponseCode::error); }

            tasks.emplace_back(
                0,
                queue_task(
                    api,
                    sender,
                    std::to_string(taskID),
                    [&](const auto& in, auto& out) -> void {
                        evaluate_send_payment_cheque(in, out);
                    },
                    std::move(future)));

            return reply(ResponseCode::queued);
        }
        case PaymentType::transfer: {
            const auto destination =
                api.Factory().IdentifierFromBase58(in.DestinationAccount());
            const auto notary = api.Storage().AccountServer(source);
            auto [taskID, future] = otx.SendTransfer(
                sender, notary, source, destination, in.Amount(), in.Memo());

            if (0 == taskID) { return reply(ResponseCode::error); }

            tasks.emplace_back(
                0,
                queue_task(
                    api,
                    sender,
                    std::to_string(taskID),
                    [&](const auto& in, auto& out) -> void {
                        evaluate_send_payment_transfer(api, in, out);
                    },
                    std::move(future)));

            return reply(ResponseCode::queued);
        }
        case PaymentType::voucher:
        case PaymentType::invoice:
        case PaymentType::blinded:
        default: {
            return std::make_unique<response::SendPayment>(
                in,
                response::Base::Responses{{0, ResponseCode::unimplemented}},
                response::Base::Tasks{});
        }
    }
}
}  // namespace opentxs::rpc::implementation
