// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                  // IWYU pragma: associated
#include "1_Internal.hpp"                // IWYU pragma: associated
#include "blockchain/p2p/peer/Peer.hpp"  // IWYU pragma: associated

#include <chrono>
#include <stdexcept>
#include <string_view>

#include "blockchain/DownloadTask.hpp"
#include "blockchain/p2p/peer/ConnectionManager.hpp"
#include "internal/blockchain/database/Peer.hpp"
#include "internal/blockchain/node/BlockOracle.hpp"
#include "internal/blockchain/node/Config.hpp"
#include "internal/blockchain/node/FilterOracle.hpp"
#include "internal/blockchain/node/PeerManager.hpp"
#include "internal/util/Future.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/network/asio/Socket.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/util/Log.hpp"
#include "util/ScopeGuard.hpp"
#include "util/Work.hpp"
#include "util/threadutil.hpp"

namespace zmq = opentxs::network::zeromq;

namespace opentxs::blockchain::p2p::implementation
{
Peer::Peer(
    const api::Session& api,
    const node::internal::Config& config,
    const node::internal::Mempool& mempool,
    const node::internal::Manager& network,
    const node::internal::FilterOracle& filter,
    const node::internal::BlockOracle& block,
    const node::internal::PeerManager& manager,
    database::Peer& database,
    const int id,
    const UnallocatedCString& shutdown,
    const std::size_t headerSize,
    const std::size_t bodySize,
    std::unique_ptr<internal::Address> address) noexcept
    : Worker(api, "Peer")
    , network_(network)
    , filter_(filter)
    , block_(block)
    , manager_(manager)
    , database_(database)
    , mempool_(mempool)
    , log_(LogTrace())
    , chain_(address->Chain())
    , display_chain_(print(chain_))
    , header_checkpoint_verified_(false)
    , cfheader_checkpoint_verified_(false)
    , address_(std::move(address))
    , download_peers_()
    , state_()
    , cfheader_job_()
    , cfilter_job_()
    , block_job_()
    , block_batch_()
    , known_transactions_()
    , init_start_(Clock::now())
    , verify_filter_checkpoint_(config.download_cfilters_)
    , id_(id)
    , shutdown_endpoint_(shutdown)
    , untrusted_connection_id_(pipeline_.ConnectionIDDealer())
    , connection_(init_connection_manager(
          api_,
          id_,
          manager_,
          *this,
          running_,
          address_,
          headerSize))
    , send_promises_()
    , activity_(api_, pipeline_)
    , init_promise_()
    , init_(init_promise_.get_future())
    , last_job_{}
{
    OT_ASSERT(connection_);

    connection_->init();
    init_executor({manager_.Endpoint(Task::Heartbeat), shutdown_endpoint_});
    start();
    trigger();
}

auto Peer::activity_timeout() noexcept -> void
{
    log_("Disconnecting ")(display_chain_)(" peer ")(address_.Display())(
        " due to activity timeout.")
        .Flush();
    disconnect();
}

auto Peer::break_promises() noexcept -> void
{
    state_.break_promises();
    send_promises_.Break();
}

auto Peer::check_download_peers() noexcept -> void
{
    const auto interval = Clock::now() - download_peers_.get();
    const bool download =
        std::chrono::minutes(peer_download_interval_) <= interval;

    if (download) { request_addresses(); }
}

auto Peer::check_init() noexcept -> void
{
    if (connection_->is_initialized()) {
        log_(display_chain_)(" peer ")(address_.Display())(" initialized")
            .Flush();
        state_.value_.store(State::Connect);
        connect();
    } else {
        static constexpr auto limit = 30s;
        const auto elapsed = Clock::now() - init_start_;

        if (limit <= elapsed) {
            log_("Disconnecting ")(display_chain_)(" peer ")(
                address_.Display())(" due to connection timeout.")
                .Flush();
            disconnect();
        }
    }
}

auto Peer::check_jobs() noexcept -> void
{
    constexpr auto limit = std::chrono::minutes{2};

    if (auto& job = cfheader_job_; job) {
        if (job.Elapsed() >= limit) { reset_cfheader_job(); }
    } else if (cfheader_checkpoint_verified_) {
        reset_cfheader_job();
    }

    if (auto& job = cfilter_job_; job) {
        if (job.Elapsed() >= limit) { reset_cfilter_job(); }
    } else if (cfheader_checkpoint_verified_) {
        reset_cfilter_job();
    }

    if (auto& job = block_batch_; job.has_value()) {
        if (const auto elapsed = job->LastActivity(); elapsed >= limit) {
            log_(OT_PRETTY_CLASS())("cancelling block download batch ")(
                job->ID())(" due to ")(
                std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed))(
                " of inactivity")
                .Flush();
            reset_block_batch();
        } else {
            log_(OT_PRETTY_CLASS())("block download batch ")(job->ID())(
                " is running and will not time out until ")(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    limit - elapsed))(" of inactivity")
                .Flush();
        }
    } else if (header_checkpoint_verified_) {
        log_(OT_PRETTY_CLASS())(
            "requesting block download batch from block oracle")
            .Flush();
        reset_block_batch();
    }

    if (auto& job = block_job_; job) {
        if (job.Elapsed() >= limit) { reset_block_job(); }
    } else if (header_checkpoint_verified_) {
        reset_block_job();
    }
}

auto Peer::check_handshake() noexcept -> void
{
    tdiag("check_handshake");
    auto& state = state_.handshake_;

    if (state.first_action_ && state.second_action_ &&
        (false == state.done())) {
        log_(
            address_.Incoming() ? "Incoming connection from "
                                : "Connected to ")(print(address_.Chain()))(
            " peer at ")(address_.Display())
            .Flush();
        log_("Advertised services: ").Flush();

        for (const auto& service : address_.Services()) {
            log_(" * ")(p2p::DisplayService(service)).Flush();
        }

        update_address_activity();
        state.promise_.set_value();

        OT_ASSERT(state.done());
    }

    trigger();
}

auto Peer::check_verify() noexcept -> void
{
    tdiag("check_verify");
    auto& state = state_.verify_;

    if (state.first_action_ &&
        (state.second_action_ || (false == verify_filter_checkpoint_)) &&
        (false == state.done())) {
        state.promise_.set_value();
    }

    trigger();
}

auto Peer::connect() noexcept -> void { connection_->connect(); }

auto Peer::disconnect() noexcept -> void
{
    try {
        state_.connect_.promise_.set_value(false);
    } catch (...) {
    }

    log_(
        address_.Incoming() ? "Dropping incoming connection "
                            : "Disconnecting from ")(connection_->host())(":")(
        connection_->port())
        .Flush();
    manager_.Disconnect(id_);
}

auto Peer::init() noexcept -> void { init_promise_.set_value(); }

auto Peer::init_connection_manager(
    const api::Session& api,
    const int id,
    const node::internal::PeerManager& manager,
    Peer& parent,
    const std::atomic<bool>& running,
    const peer::Address& address,
    const std::size_t headerSize) noexcept
    -> std::unique_ptr<peer::ConnectionManager>
{
    if (Network::zmq == address.Type()) {
        if (address.Incoming()) {

            return peer::ConnectionManager::ZMQIncoming(
                api,
                id,
                parent,
                parent.pipeline_,
                running,
                address,
                headerSize);
        } else {

            return peer::ConnectionManager::ZMQ(
                api,
                id,
                parent,
                parent.pipeline_,
                running,
                address,
                headerSize);
        }
    } else {
        if (address.Incoming()) {

            return peer::ConnectionManager::TCPIncoming(
                api,
                id,
                parent,
                parent.pipeline_,
                running,
                address,
                headerSize,
                manager.LookupIncomingSocket(id));
        } else {

            return peer::ConnectionManager::TCP(
                api,
                id,
                parent,
                parent.pipeline_,
                running,
                address,
                headerSize);
        }
    }
}

auto Peer::on_connect() noexcept -> void
{
    tdiag("on_connect");
    log_(display_chain_)(" peer ")(address_.Display())(" connected").Flush();

    try {
        state_.connect_.promise_.set_value(true);
    } catch (...) {
    }

    state_.value_.store(
        address_.Incoming() ? State::Listening : State::Handshake);
    trigger();
}

auto Peer::on_init() noexcept -> void { check_init(); }

auto Peer::pipeline(zmq::Message&& message) noexcept -> void
{
    MessageMarker m(message);
    if (m) { tdiag("QQQ message from ", to_string(m)); }

    if (false == IsReady(init_)) {
        pipeline_.Push(std::move(message));

        tdiag("Peer::pipeline pushed back, no init");
        return;
    }

    if (false == running_.load()) {
        tdiag("Peer::pipeline ignored, not running");
        return;
    }

    const auto header = message.Header();
    const auto body = message.Body();

    OT_ASSERT(0 < header.size());
    OT_ASSERT(0 < body.size());

    const auto connectionID = header.at(0).as<std::size_t>();
    const auto task = [&] {
        try {

            return body.at(0).as<Task>();
        } catch (const std::exception& e) {
            LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
            // TODO It's impossible for this exception to happen but it does
            // anyway from time to time. Somebody really ought to figure out why
            // someday.

            return Task::Disconnect;
        }
    }();

    last_job_ = task;

    if (connectionID == untrusted_connection_id_) {
        tdiag("Peer::pipeline untrusted");
        if (false == connection_->filter(task)) {
            LogError()(OT_PRETTY_CLASS())("Peer ")(address_.Display())(
                " sent an internal control message instead of a valid protocol "
                "message")
                .Flush();
            disconnect();

            return;
        }
    }

    switch (task) {
        case Task::Shutdown: {
            tdiag("Peer::pipeline Shutdown");
            protect_shutdown([this] { shut_down(); });
        } break;
        case Task::Mempool: {
            tdiag("Peer::pipeline Mempool");
            process_mempool(message);
        } break;
        case Task::Register: {
            tdiag("Peer::pipeline Register");
            connection_->on_register(std::move(message));
        } break;
        case Task::Connect: {
            tdiag("Peer::pipeline Connect");
            connection_->on_connect(std::move(message));
        } break;
        case Task::Disconnect: {
            tdiag("Peer::pipeline Disconnect");
            disconnect();
        } break;
        case Task::Getheaders: {
            tdiag("Peer::pipeline Getheaders");
            if (State::Run == state_.value_.load()) { request_headers(); }
        } break;
        case Task::Getblock: {
            tdiag("Peer::pipeline Getblock");
            request_block(std::move(message));
        } break;
        case Task::BroadcastTransaction: {
            tdiag("Peer::pipeline BroadcastTransaction");
            broadcast_transaction(std::move(message));
        } break;
        case Task::BroadcastBlock: {
            tdiag("Peer::pipeline BroadcastBlock");
            broadcast_block(std::move(message));
        } break;
        case Task::JobAvailableCfheaders: {
            //            tdiag("Peer::pipeline JobAvailableCfheaders");
            if (State::Run == state_.value_.load()) {
                if (cfheader_job_) { break; }

                reset_cfheader_job();
            }
        } break;
        case Task::JobAvailableCfilters: {
            //            tdiag("Peer::pipeline JobAvailableCfilters");
            if (State::Run == state_.value_.load()) {
                if (cfilter_job_) { break; }

                reset_cfilter_job();
            }
        } break;
        case Task::JobAvailableBlock: {
            tdiag("Peer::pipeline JobAvailableBlock");
            if (State::Run == state_.value_.load()) {
                if (block_job_) { break; }

                reset_block_job();
            }
        } break;
        case Task::ActivityTimeout: {
            tdiag("Peer::pipeline ActivityTimeout");
            activity_timeout();
        } break;
        case Task::NeedPing: {
            tdiag("Peer::pipeline NeedPing");
            if (State::Run == state_.value_.load()) { ping(); }
        } break;
        case Task::Body: {
            tdiag("Peer::pipeline Body");
            activity_.Bump();
            connection_->on_body(std::move(message));
        } break;
        case Task::Header: {
            tdiag("Peer::pipeline Header");
            activity_.Bump();
            connection_->on_header(std::move(message));
        } break;
        case Task::P2P: {
            tdiag("Peer::pipeline P2P");
            activity_.Bump();
            process_message(std::move(message));
        } break;
        case Task::ReceiveMessage: {
            tdiag("Peer::pipeline ReceiveMessage");
            activity_.Bump();
            process_message(std::move(message));
        } break;
        case Task::SendMessage: {
            tdiag("Peer::pipeline SendMessage");
            transmit(std::move(message));
        } break;
        case Task::Init: {
            tdiag("Peer::pipeline Init");
            connection_->on_init(std::move(message));
        } break;
        case Task::StateMachine: {
            tdiag("Peer::pipeline StateMachine");
            [[fallthrough]];
            case Task::Heartbeat:
                do_work();
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto Peer::process_mempool(const zmq::Message& message) noexcept -> void
{
    const auto body = message.Body();

    if (body.at(1).as<Type>() != chain_) { return; }

    const auto hash = UnallocatedCString{body.at(2).Bytes()};

    if (0 < known_transactions_.count(hash)) { return; }

    broadcast_inv_transaction(hash);
}

auto Peer::process_state_machine() noexcept -> void
{
    switch (state_.value_.load()) {
        case State::Init: {
            check_init();
        } break;
        case State::Run: {
            check_jobs();
            check_download_peers();
        } break;
        default: {
        }
    }
}

auto Peer::reset_block_batch() noexcept -> void
{
    auto& job = block_batch_;
    job.reset();

    if (false == header_checkpoint_verified_) { return; }
    if (block_job_) { return; }

    job.emplace(block_.GetBlockBatch());

    OT_ASSERT(job.has_value());

    if (0u == job->Remaining()) {
        log_(OT_PRETTY_CLASS())(
            "block oracle does not have block download work")
            .Flush();
        job.reset();
    } else {
        log_(OT_PRETTY_CLASS())("received block download job ")(job->ID())
            .Flush();
        request_blocks();
    }
}

auto Peer::reset_block_job() noexcept -> void
{
    auto& job = block_job_;
    job = {};

    if (false == header_checkpoint_verified_) { return; }
    if (block_batch_.has_value()) { return; }

    job = block_.GetBlockJob();

    if (job) {
        log_(OT_PRETTY_CLASS())("received block download job ")(job.id_)
            .Flush();
        request_blocks();
    } else {
        log_(OT_PRETTY_CLASS())(
            "block oracle does not have block download work")
            .Flush();
    }
}

auto Peer::reset_cfheader_job() noexcept -> void
{
    auto& job = cfheader_job_;
    job = {};

    if (cfheader_checkpoint_verified_) { job = filter_.GetHeaderJob(); }

    if (job) { request_cfheaders(); }
}

auto Peer::reset_cfilter_job() noexcept -> void
{
    auto& job = cfilter_job_;
    job = {};

    if (cfheader_checkpoint_verified_) { job = filter_.GetFilterJob(); }

    if (job) { request_cfilter(); }
}

auto Peer::send(std::pair<zmq::Frame, zmq::Frame>&& frames, bool diag) noexcept
    -> SendStatus
{
    try {
        if (false == state_.connect_.future_.get()) {
            log_(OT_PRETTY_CLASS())("Unable to send to disconnected peer")
                .Flush();

            return {};
        }
    } catch (...) {
        return {};
    }

    if (running_.load()) {
        auto data = send_promises_.NewPromise();
        pipeline_.Push([&] {
            auto& [future, promise] = data;
            auto& [header, payload] = frames;
            auto out = MakeWork(Task::SendMessage);
            out.AddFrame(std::move(header));
            out.AddFrame(std::move(payload));
            out.AddFrame(promise);

            if (diag) { MessageMarker().mark(out); }
            return out;
        }());
        auto& [future, promise] = data;

        return std::move(future);
    } else {
        return {};
    }
}

auto Peer::Shutdown() noexcept -> void
{
    tdiag("Shutdown");
    protect_shutdown([this] { shut_down(); });
}

auto Peer::shut_down() noexcept -> void
{
    tdiag("shut_down");
    // Avoid last message sneaking through to reactor after sending
    // socket has been closed by pipeline, so stop() first.
    stop();
    close_pipeline();
    const auto state = state_.value_.exchange(State::Shutdown);
    const auto addr = address_.Display();
    connection_->stop_external();
    break_promises();
    connection_->shutdown_external();

    if ((State::Handshake != state) && (State::Listening != state)) {
        update_address_activity();
    }

    log_("Disconnected from ")(addr).Flush();
    // TODO MT-34 investigate what other actions might be needed
}

auto Peer::start_verify() noexcept -> void
{
    if (address_.Incoming()) {
        log_(OT_PRETTY_CLASS())("incoming peer ")(address_.Display())(
            " is not required to validate checkpoints")
            .Flush();
        state_.value_.store(State::Subscribe);
        do_work();
    } else {
        request_checkpoint_block_header();

        if (verify_filter_checkpoint_) {
            log_(OT_PRETTY_CLASS())("outgoing peer ")(address_.Display())(
                " must validate block header and cfheader checkpoints")
                .Flush();
            request_checkpoint_filter_header();
        } else {
            log_(OT_PRETTY_CLASS())("outgoing peer ")(address_.Display())(
                " must validate block header checkpoints only")
                .Flush();
        }
    }
}

auto Peer::state_machine() noexcept -> int
{
    tdiag("Peer::state_machine");
    log_(OT_PRETTY_CLASS()).Flush();

    if (false == running_.load()) { return -1; }

    try {
        switch (state_.value_.load()) {
            case State::Listening: {
                OT_ASSERT(address_.Incoming());

                log_(OT_PRETTY_CLASS())(
                    "verifying incoming handshake protocol for ")(
                    address_.Display())
                    .Flush();
                static constexpr auto timeout = 20s;
                const auto disconnect = state_.handshake_.run(
                    timeout, [] {}, State::Verify);

                if (disconnect) {
                    throw std::runtime_error{
                        UnallocatedCString{
                            "failure to complete handshake within "} +
                        std::to_string(timeout.count()) +
                        " seconds (incoming)"};
                }
            } break;
            case State::Handshake: {
                log_(OT_PRETTY_CLASS())(
                    "verifying outgoing handshake protocol for ")(
                    address_.Display())
                    .Flush();
                static constexpr auto timeout = 20s;
                const auto disconnect = state_.handshake_.run(
                    timeout, [this] { start_handshake(); }, State::Verify);

                if (disconnect) {
                    throw std::runtime_error{
                        UnallocatedCString{
                            "failure to complete handshake within "} +
                        std::to_string(timeout.count()) +
                        " seconds (outgoing)"};
                }
            } break;
            case State::Verify: {
                log_(OT_PRETTY_CLASS())("verifying checkpoints for ")(
                    address_.Display())
                    .Flush();
                static constexpr auto timeout = 30s;
                const auto disconnect = state_.verify_.run(
                    timeout, [this] { start_verify(); }, State::Subscribe);

                if (disconnect) {
                    throw std::runtime_error{
                        UnallocatedCString{
                            "failure to respond to checkpoint requests "
                            "within "} +
                        std::to_string(timeout.count()) + " seconds"};
                }
            } break;
            case State::Subscribe: {
                log_(OT_PRETTY_CLASS())("achieved subscribe state for ")(
                    address_.Display())
                    .Flush();
                subscribe();
                state_.value_.store(State::Run);
                manager_.VerifyPeer(id_, address_.Display());
                [[fallthrough]];
            }
            case State::Run: {
                log_(OT_PRETTY_CLASS())("achieved run state for ")(
                    address_.Display())
                    .Flush();
                process_state_machine();
            } break;
            case State::Shutdown: {
                throw std::runtime_error{"session shutdown in progress"};
            }
            default: {
            }
        }
    } catch (const std::exception& e) {
        log_("Disconnecting ")(display_chain_)(" peer ")(address_.Display())(
            " due to ")(e.what())
            .Flush();
        disconnect();
    }

    return -1;
}

auto Peer::last_job_str() const noexcept -> std::string
{
    return std::string(print(last_job_));
}

auto Peer::subscribe() noexcept -> void
{
    const auto network =
        (1 == address_.Services().count(p2p::Service::Network));
    const auto limited =
        (1 == address_.Services().count(p2p::Service::Limited));
    const auto cfilter =
        (1 == address_.Services().count(p2p::Service::CompactFilters));
    const auto bloom = (1 == address_.Services().count(p2p::Service::Bloom));

    if (network || limited || header_checkpoint_verified_) {
        pipeline_.PullFrom(manager_.Endpoint(Task::Getheaders));
        pipeline_.PullFrom(manager_.Endpoint(Task::Getblock));
        pipeline_.PullFrom(manager_.Endpoint(Task::BroadcastTransaction));
        pipeline_.SubscribeTo(manager_.Endpoint(Task::JobAvailableBlock));
    }
    if (cfilter || cfheader_checkpoint_verified_) {
        pipeline_.SubscribeTo(manager_.Endpoint(Task::JobAvailableCfheaders));
        pipeline_.SubscribeTo(manager_.Endpoint(Task::JobAvailableCfilters));
    }

    pipeline_.SubscribeTo(manager_.Endpoint(Task::BroadcastBlock));
    pipeline_.SubscribeTo(api_.Endpoints().BlockchainMempool());
    request_headers();
    request_addresses();
    if (bloom) { request_mempool(); }
}

auto Peer::transmit(zmq::Message&& message) noexcept -> void
{
    if (false == running_.load()) { return; }

    OT_ASSERT(message.Body().size() > 3)

    auto body = message.Body();
    auto& header = body.at(1);
    auto& payload = body.at(2);
    const auto& promiseFrame = body.at(3);
    const auto index = promiseFrame.as<int>();
    auto success = bool{false};
    const auto payload_size = payload.size();
    auto postcondition =
        ScopeGuard{[&] { send_promises_.SetPromise(index, success); }};
    log_(OT_PRETTY_CLASS())("Sending ")(header.size() + payload_size)(
        " byte message:")
        .Flush();
    LogInsane()(Data::Factory(header)->asHex())(Data::Factory(payload)->asHex())
        .Flush();
    auto promise = std::make_unique<peer::ConnectionManager::SendPromise>();

    OT_ASSERT(promise);

    auto future = promise->get_future();
    connection_->transmit(
        std::move(header), std::move(payload), std::move(promise));
    auto result{false};
    const auto start = Clock::now();
    static const auto limit = 10s;

    try {
        while (running_.load()) {
            const auto status = future.wait_for(5ms);

            if (std::future_status::ready == status) {
                result = future.get();

                break;
            } else if (const auto time = Clock::now() - start; time >= limit) {
                log_("Disconnecting ")(display_chain_)(" peer ")(
                    address_.Display())(" due to transmit timeout.")
                    .Flush();
                disconnect();

                return;
            }
        }
    } catch (const std::exception& e) {
        log_("Disconnecting ")(display_chain_)(" peer ")(address_.Display())(
            " due to transmit error: ")(e.what())
            .Flush();
        disconnect();

        return;
    }

    if (result) {
        log_(OT_PRETTY_CLASS())("Sent ")(payload_size)(" bytes").Flush();
        success = true;
    } else {
        log_("Disconnecting ")(display_chain_)(" peer ")(address_.Display())(
            " due to unspecified transmit error.")
            .Flush();
        success = false;
        disconnect();
    }
}

auto Peer::update_address_activity() noexcept -> void
{
    auto addr(address_.UpdateTime(activity_.get()));
    database_.AddOrUpdate(std::move(addr));
}

auto Peer::update_address_services(
    const UnallocatedSet<p2p::Service>& services) noexcept -> void
{
    database_.AddOrUpdate(address_.UpdateServices(services));
}

Peer::~Peer()
{
    protect_shutdown([this] { shut_down(); });
}
}  // namespace opentxs::blockchain::p2p::implementation
