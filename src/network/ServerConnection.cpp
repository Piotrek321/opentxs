// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                  // IWYU pragma: associated
#include "1_Internal.hpp"                // IWYU pragma: associated
#include "network/ServerConnection.hpp"  // IWYU pragma: associated

#include <ServerReply.pb.h>
#include <ServerRequest.pb.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <memory>
#include <ratio>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include "Proto.hpp"
#include "Proto.tpp"
#include "internal/api/session/Endpoints.hpp"
#include "internal/api/session/FactoryAPI.hpp"
#include "internal/network/zeromq/message/Message.hpp"
#include "internal/otx/common/Message.hpp"
#include "internal/serialization/protobuf/Check.hpp"
#include "internal/serialization/protobuf/verify/ServerReply.hpp"
#include "internal/util/Flag.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/ZMQ.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/core/Armored.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/core/contract/ServerContract.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/identity/Types.hpp"
#include "opentxs/network/ServerConnection.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/curve/Client.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Dealer.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/network/zeromq/socket/Request.hpp"
#include "opentxs/network/zeromq/socket/Socket.hpp"
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/otx/Reply.hpp"
#include "opentxs/otx/Request.hpp"
#include "opentxs/otx/ServerRequestType.hpp"
#include "opentxs/otx/consensus/Server.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/Thread.hpp"

namespace zmq = opentxs::network::zeromq;

namespace opentxs::network
{
auto ServerConnection::Factory(
    const api::Session& api,
    const api::network::ZMQ& zmq,
    const zeromq::socket::Publish& updates,
    const OTServerContract& contract) -> ServerConnection
{
    return {new ServerConnection::Imp(api, zmq, updates, contract)};
}

ServerConnection::ServerConnection(Imp* imp) noexcept
    : imp_(imp)
{
    OT_ASSERT(nullptr != imp);
}

ServerConnection::ServerConnection(ServerConnection&& rhs) noexcept
    : imp_{nullptr}
{
    swap(rhs);
}

auto ServerConnection::operator=(ServerConnection&& rhs) noexcept
    -> ServerConnection&
{
    swap(rhs);

    return *this;
}

ServerConnection::~ServerConnection()
{
    if (nullptr != imp_) {
        delete imp_;
        imp_ = nullptr;
    }
}

auto ServerConnection::swap(ServerConnection& rhs) noexcept -> void
{
    std::swap(imp_, rhs.imp_);
}

auto ServerConnection::ChangeAddressType(const AddressType type) -> bool
{
    return imp_->ChangeAddressType(type);
}

auto ServerConnection::ClearProxy() -> bool { return imp_->ClearProxy(); }

auto ServerConnection::EnableProxy() -> bool { return imp_->EnableProxy(); }

auto ServerConnection::Send(
    const otx::context::Server& context,
    const Message& message,
    const PasswordPrompt& reason,
    const Push push) -> otx::client::NetworkReplyMessage
{
    return imp_->Send(context, message, reason, push);
}
auto ServerConnection::Status() const -> bool { return imp_->Status(); }

ServerConnection::Imp::Imp(
    const api::Session& api,
    const api::network::ZMQ& zmq,
    const zeromq::socket::Publish& updates,
    const OTServerContract& contract)
    : zmq_(zmq)
    , api_(api)
    , updates_(updates)
    , server_id_(api_.Factory().NotaryIDFromBase58(
          contract->ID().asBase58(api_.Crypto())))
    , address_type_(zmq.DefaultAddressType())
    , remote_contract_(contract)
    , thread_()
    , callback_(zeromq::ListenCallback::Factory(
          [=](const auto& in) { process_incoming(in); }))
    , registration_socket_(zmq.Context().DealerSocket(
          callback_,
          zmq::socket::Direction::Connect,
          serverConnectionRegistrationThreadName))
    , socket_(zmq.Context().RequestSocket())
    , notification_socket_(
          zmq.Context().PushSocket(zmq::socket::Direction::Connect))
    , last_activity_(std::time(nullptr))
    , sockets_ready_(Flag::Factory(false))
    , status_(Flag::Factory(false))
    , use_proxy_(Flag::Factory(false))
    , registration_lock_()
    , registered_for_push_()
{
    thread_ = std::thread(&Imp::activity_timer, this);
    const auto started = notification_socket_->Start(
        api_.Endpoints().Internal().ProcessPushNotification().data());

    OT_ASSERT(started);
}

ServerConnection::Imp::~Imp()
{
    if (thread_.joinable()) { thread_.join(); }
}

auto ServerConnection::Imp::activity_timer() -> void
{
    while (zmq_.Running()) {
        const auto limit = zmq_.KeepAlive();
        const auto now = std::chrono::seconds(std::time(nullptr));
        const auto last = std::chrono::seconds(last_activity_.load());
        const auto duration = now - last;

        if (duration > limit) {
            if (limit > 0s) {
                const auto result = socket_->Send(zeromq::Message{});

                if (otx::client::SendResult::TIMEOUT != result.first) {
                    reset_timer();

                    if (status_->On()) { publish(); }
                }
            } else {
                if (status_->Off()) { publish(); }
            }
        }

        Sleep(1s);
    }
}

auto ServerConnection::Imp::async_socket(const Lock& lock) const
    -> OTZMQDealerSocket
{
    auto output = zmq_.Context().DealerSocket(
        callback_,
        zmq::socket::Direction::Connect,
        serverConnectionAsyncThreadName);
    set_proxy(lock, output);
    set_timeouts(lock, output);
    set_curve(lock, output);
    output->Start(endpoint());

    return output;
}

auto ServerConnection::Imp::ChangeAddressType(const AddressType type) -> bool
{
    Lock lock(lock_);
    address_type_ = type;
    reset_socket(lock);

    return true;
}

auto ServerConnection::Imp::ClearProxy() -> bool
{
    Lock lock(lock_);
    use_proxy_->Off();
    reset_socket(lock);

    return true;
}

auto ServerConnection::Imp::EnableProxy() -> bool
{
    Lock lock(lock_);
    use_proxy_->On();
    reset_socket(lock);

    return true;
}

auto ServerConnection::Imp::disable_push(const identifier::Nym& nymID) -> void
{
    Lock registrationLock(registration_lock_);
    registered_for_push_[nymID] = true;
}

auto ServerConnection::Imp::endpoint() const -> UnallocatedCString
{
    std::uint32_t port{0};
    UnallocatedCString hostname{""};
    AddressType type{};
    const auto have =
        remote_contract_->ConnectInfo(hostname, port, type, address_type_);

    if (false == have) {
        LogError()(OT_PRETTY_CLASS())(
            "Failed retrieving connection info from server contract.")
            .Flush();

        OT_FAIL;
    }

    const auto endpoint = form_endpoint(type, hostname, port);
    LogError()("Establishing connection to: ")(endpoint).Flush();

    return endpoint;
}

auto ServerConnection::Imp::form_endpoint(
    AddressType type,
    UnallocatedCString hostname,
    std::uint32_t port) const -> UnallocatedCString
{
    auto output = std::stringstream{};

    if (AddressType::Inproc == type) {
        output << hostname;
    } else {
        output << "tcp://";
        output << hostname;
    }

    output << ":";
    output << std::to_string(port);

    return output.str();
}

auto ServerConnection::Imp::get_async(const Lock& lock)
    -> zeromq::socket::Dealer&
{
    OT_ASSERT(verify_lock(lock));

    if (false == sockets_ready_.get()) {
        registration_socket_ = async_socket(lock);
        socket_ = sync_socket(lock);
        sockets_ready_->On();
    }

    return registration_socket_;
}

auto ServerConnection::Imp::get_sync(const Lock& lock)
    -> zeromq::socket::Request&
{
    OT_ASSERT(verify_lock(lock));

    if (false == sockets_ready_.get()) {
        registration_socket_ = async_socket(lock);
        socket_ = sync_socket(lock);
        sockets_ready_->On();
    }

    return socket_;
}

auto ServerConnection::Imp::get_timeout() -> Time
{
    return Clock::now() + zmq_.SendTimeout();
}

auto ServerConnection::Imp::process_incoming(const zeromq::Message& in) -> void
{
    if (status_->On()) { publish(); }

    try {
        const auto body = in.Body();
        const auto& payload = [&]() -> auto&
        {
            if (2u > body.size()) {
                throw std::runtime_error{"payload missing"};
            }

            if ((2u == body.size()) && (0u == body.at(1).size())) {

                return body.at(0);
            }

            const auto type = [&] {
                try {

                    return body.at(0).as<WorkType>();
                } catch (...) {
                    throw std::runtime_error{"unknown message type"};
                }
            }();

            switch (type) {
                case WorkType::OTXPush: {

                    return body.at(1);
                }
                case WorkType::OTXResponse:
                default: {
                    throw std::runtime_error{"unsupported message type"};
                }
            }
        }
        ();
        const auto proto = proto::Factory<proto::ServerReply>(payload);

        if (false == proto::Validate(proto, VERBOSE)) {
            throw std::runtime_error{"invalid serialization"};
        }

        const auto message = otx::Reply::Factory(api_, proto);

        if (false == message.Validate()) {
            throw std::runtime_error{"Invalid message"};
        }

        notification_socket_->Send([&] {
            auto out = zeromq::Message{};
            out.AddFrame(payload);  // TODO std::move

            return out;
        }());
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
    }
}

auto ServerConnection::Imp::publish() const -> void
{
    updates_.Send([&] {
        const auto status = bool{status_.get()};
        auto work =
            network::zeromq::tagged_message(WorkType::OTXConnectionStatus);
        work.AddFrame(server_id_);
        work.AddFrame(status);

        return work;
    }());
}

auto ServerConnection::Imp::register_for_push(
    const otx::context::Server& context,
    const PasswordPrompt& reason) -> void
{
    if (2 > context.Request()) {
        LogVerbose()(OT_PRETTY_CLASS())("Nym is not yet registered").Flush();

        return;
    }

    Lock registrationLock(registration_lock_);
    const auto& nymID = context.Nym()->ID();
    auto& isRegistered = registered_for_push_[nymID];

    if (isRegistered) { return; }

    auto request = otx::Request::Factory(
        api_,
        context.Nym(),
        context.Notary(),
        otx::ServerRequestType::Activate,
        0,
        reason);
    request.SetIncludeNym(true, reason);
    auto message = zmq::Message{};
    message.AddFrame();
    auto serialized = proto::ServerRequest{};
    if (false == request.Serialize(serialized)) {
        LogVerbose()(OT_PRETTY_CLASS())("Failed to serialize request.").Flush();

        return;
    }
    message.Internal().AddFrame(serialized);
    message.AddFrame();
    Lock socketLock(lock_);
    isRegistered = get_async(socketLock).Send(std::move(message));
}

auto ServerConnection::Imp::reset_socket(const Lock& lock) -> void
{
    OT_ASSERT(verify_lock(lock));

    sockets_ready_->Off();
}

auto ServerConnection::Imp::reset_timer() -> void
{
    last_activity_.store(std::time(nullptr));
}

auto ServerConnection::Imp::Send(
    const otx::context::Server& context,
    const Message& message,
    const PasswordPrompt& reason,
    const Push push) -> otx::client::NetworkReplyMessage
{
    struct Cleanup {
        const Lock& lock_;
        Imp& connection_;
        otx::client::SendResult& result_;
        std::shared_ptr<Message>& reply_;
        bool success_{false};

        void SetStatus(const otx::client::SendResult status)
        {
            if (otx::client::SendResult::VALID_REPLY == status) {
                success_ = true;
            }
        }

        Cleanup(
            const Lock& lock,
            Imp& connection,
            otx::client::SendResult& result,
            std::shared_ptr<Message>& reply)
            : lock_(lock)
            , connection_(connection)
            , result_(result)
            , reply_(reply)
        {
        }

        ~Cleanup()
        {
            if (false == success_) {
                connection_.reset_socket(lock_);
                reply_.reset();
            }
        }
    };

    if (Push::Enable == push) {
        LogTrace()(OT_PRETTY_CLASS())("Registering for push").Flush();
        register_for_push(context, reason);
    } else {
        LogTrace()(OT_PRETTY_CLASS())("Skipping push").Flush();
        disable_push(context.Nym()->ID());
    }

    otx::client::NetworkReplyMessage output{
        otx::client::SendResult::Error, nullptr};
    auto& status = output.first;
    auto& reply = output.second;
    reply.reset(api_.Factory().InternalSession().Message().release());

    OT_ASSERT(false != bool(reply));

    auto raw = String::Factory();
    message.SaveContractRaw(raw);
    auto envelope = Armored::Factory(raw);

    if (false == envelope->Exists()) {
        LogError()(OT_PRETTY_CLASS())("Failed to armor message").Flush();

        return output;
    }

    Lock socketLock(lock_);
    Cleanup cleanup(socketLock, *this, status, reply);
    auto sendresult = get_sync(socketLock).Send([&] {
        auto out = zeromq::Message{};
        out.AddFrame(envelope->Get());

        return out;
    }());

    if (status_->On()) { publish(); }

    status = sendresult.first;
    auto in = sendresult.second;
    auto replymessage{api_.Factory().InternalSession().Message()};

    OT_ASSERT(false != bool(replymessage));

    if (otx::client::SendResult::TIMEOUT == status) {
        LogError()(OT_PRETTY_CLASS())("Reply timeout.").Flush();
        cleanup.SetStatus(otx::client::SendResult::TIMEOUT);

        return output;
    }

    try {
        const auto body = in.Body();
        const auto& payload = [&] {
            if (0u == body.size()) {
                throw std::runtime_error{"Empty reply"};
            } else if (1u == body.size()) {

                return body.at(0);
            } else if (0u == body.at(1).size()) {
                throw std::runtime_error{
                    "Push notification received as a reply"};
            } else {
                const auto type = [&] {
                    try {

                        return body.at(0).as<WorkType>();
                    } catch (...) {
                        throw std::runtime_error{"Unknown message type"};
                    }
                }();

                switch (type) {
                    case WorkType::OTXLegacyXML: {

                        return body.at(1);
                    }
                    default: {
                        throw std::runtime_error{"Unsupported message type"};
                    }
                }
            }
        }();

        if (0 == payload.size()) {
            throw std::runtime_error{"Invalid reply message"};
        }

        const auto serialized = [&] {
            const auto armored = [&] {
                auto out = Armored::Factory();
                out->Set(UnallocatedCString{payload.Bytes()}.c_str());

                return out;
            }();
            auto out = String::Factory();
            armored->GetString(out);

            return out;
        }();
        const auto loaded = replymessage->LoadContractFromString(serialized);

        if (loaded) {
            reply = std::move(replymessage);
        } else {
            LogError()(OT_PRETTY_CLASS())(
                "Received server reply, but unable to instantiate it as a "
                "Message.")
                .Flush();
            cleanup.SetStatus(otx::client::SendResult::INVALID_REPLY);
        }

        cleanup.SetStatus(otx::client::SendResult::VALID_REPLY);
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
        cleanup.SetStatus(otx::client::SendResult::INVALID_REPLY);
    }

    return output;
}

auto ServerConnection::Imp::set_curve(
    const Lock& lock,
    zeromq::curve::Client& socket) const -> void
{
    OT_ASSERT(verify_lock(lock));

    const auto set = socket.SetServerPubkey(remote_contract_);

    OT_ASSERT(set);
}

auto ServerConnection::Imp::set_proxy(
    const Lock& lock,
    zeromq::socket::Dealer& socket) const -> void
{
    OT_ASSERT(verify_lock(lock));

    if (false == use_proxy_.get()) { return; }

    auto proxy = zmq_.SocksProxy();

    if (false == proxy.empty()) {
        LogError()(OT_PRETTY_CLASS())("Setting proxy to ")(proxy).Flush();
        const auto set = socket.SetSocksProxy(proxy);

        OT_ASSERT(set);
    }
}

auto ServerConnection::Imp::set_timeouts(
    const Lock& lock,
    zeromq::socket::Socket& socket) const -> void
{
    OT_ASSERT(verify_lock(lock));

    const auto set = socket.SetTimeouts(
        zmq_.Linger(), zmq_.SendTimeout(), zmq_.ReceiveTimeout());

    OT_ASSERT(set);
}

auto ServerConnection::Imp::sync_socket(const Lock& lock) const
    -> OTZMQRequestSocket
{
    auto output = zmq_.Context().RequestSocket();
    set_timeouts(lock, output);
    set_curve(lock, output);
    output->Start(endpoint());

    return output;
}

auto ServerConnection::Imp::Status() const -> bool { return status_.get(); }
}  // namespace opentxs::network
