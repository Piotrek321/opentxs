// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                         // IWYU pragma: associated
#include "1_Internal.hpp"                       // IWYU pragma: associated
#include "network/blockchain/bitcoin/Peer.hpp"  // IWYU pragma: associated

#include <boost/smart_ptr/make_shared.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <algorithm>
#include <chrono>
#include <future>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "blockchain/DownloadTask.hpp"
#include "blockchain/bitcoin/Inventory.hpp"
#include "blockchain/bitcoin/p2p/Header.hpp"
#include "blockchain/bitcoin/p2p/message/Cmpctblock.hpp"
#include "blockchain/bitcoin/p2p/message/Feefilter.hpp"
#include "blockchain/bitcoin/p2p/message/Getblocks.hpp"
#include "blockchain/bitcoin/p2p/message/Getblocktxn.hpp"
#include "blockchain/bitcoin/p2p/message/Merkleblock.hpp"
#include "blockchain/bitcoin/p2p/message/Reject.hpp"
#include "blockchain/bitcoin/p2p/message/Sendcmpct.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/Params.hpp"
#include "internal/blockchain/bitcoin/block/Transaction.hpp"
#include "internal/blockchain/database/Peer.hpp"
#include "internal/blockchain/node/BlockBatch.hpp"
#include "internal/blockchain/node/Config.hpp"
#include "internal/blockchain/node/HeaderOracle.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/blockchain/node/Mempool.hpp"
#include "internal/blockchain/node/Types.hpp"
#include "internal/blockchain/p2p/P2P.hpp"
#include "internal/blockchain/p2p/bitcoin/Factory.hpp"
#include "internal/blockchain/p2p/bitcoin/message/Message.hpp"
#include "internal/network/blockchain/ConnectionManager.hpp"
#include "internal/network/blockchain/bitcoin/Factory.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/util/Future.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/P0330.hpp"
#include "network/blockchain/bitcoin/Peer.tpp"
#include "opentxs/OT.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/block/Header.hpp"
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/GCS.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/Header.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/blockchain/node/Types.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/FixedByteArray.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/BlockchainProfile.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Iterator.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Types.hpp"
#include "util/ScopeGuard.hpp"
#include "util/Work.hpp"

namespace opentxs::factory
{
auto BlockchainPeerBitcoin(
    const api::Session& api,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Manager& network,
    const blockchain::node::internal::PeerManager& parent,
    const blockchain::node::internal::Mempool& mempool,
    const blockchain::node::HeaderOracle& header,
    const blockchain::node::BlockOracle& block,
    const blockchain::node::FilterOracle& filter,
    const blockchain::p2p::bitcoin::Nonce& nonce,
    blockchain::database::Peer& database,
    int peerID,
    std::unique_ptr<blockchain::p2p::internal::Address> address,
    std::string_view fromParent)
    -> boost::shared_ptr<network::blockchain::internal::Peer::Imp>
{
    OT_ASSERT(address);

    using Network = opentxs::blockchain::p2p::Network;
    using ReturnType = opentxs::network::blockchain::bitcoin::Peer;

    switch (address->Type()) {
        case Network::ipv6:
        case Network::ipv4:
        case Network::zmq: {
        } break;
        default: {
            OT_FAIL;
        }
    }

    const auto chain = address->Chain();
    const auto& asio = api.Network().ZeroMQ().Internal();
    const auto batchID = asio.PreallocateBatch();
    // TODO the version of libc++ present in android ndk 23.0.7599858
    // has a broken std::allocate_shared function so we're using
    // boost::shared_ptr instead of std::shared_ptr

    return boost::allocate_shared<ReturnType>(
        alloc::PMR<ReturnType>{asio.Alloc(batchID)},
        api,
        config,
        network,
        parent,
        mempool,
        header,
        block,
        filter,
        nonce,
        database,
        chain,
        peerID,
        std::move(address),
        blockchain::params::Chains().at(chain).p2p_protocol_version_,
        fromParent,
        batchID);
}
}  // namespace opentxs::factory

namespace opentxs::network::blockchain::bitcoin
{
using namespace std::literals;

Peer::Peer(
    const api::Session& api,
    const opentxs::blockchain::node::internal::Config& config,
    const opentxs::blockchain::node::internal::Manager& network,
    const opentxs::blockchain::node::internal::PeerManager& parent,
    const opentxs::blockchain::node::internal::Mempool& mempool,
    const opentxs::blockchain::node::HeaderOracle& header,
    const opentxs::blockchain::node::BlockOracle& block,
    const opentxs::blockchain::node::FilterOracle& filter,
    const opentxs::blockchain::p2p::bitcoin::Nonce& nonce,
    opentxs::blockchain::database::Peer& database,
    opentxs::blockchain::Type chain,
    int peerID,
    std::unique_ptr<opentxs::blockchain::p2p::internal::Address> address,
    opentxs::blockchain::p2p::bitcoin::ProtocolVersion protocol,
    std::string_view fromParent,
    const zeromq::BatchID batch,
    allocator_type alloc) noexcept
    : Imp(api,
          network,
          parent,
          header,
          block,
          filter,
          database,
          chain,
          peerID,
          std::move(address),
          30s,
          1min,
          10min,
          HeaderType::Size(),
          fromParent,
          batch,
          alloc)
    , mempool_(mempool)
    , user_agent_([&] {
        auto out = CString{get_allocator()};
        out.append("/opentxs:"sv);
        out.append(VersionString());
        out.append("/"sv);

        return out;
    }())
    , peer_cfilter_([&] {
        switch (config.profile_) {
            case BlockchainProfile::desktop_native: {

                return true;
            }
            case BlockchainProfile::mobile:
            case BlockchainProfile::desktop:
            case BlockchainProfile::server: {

                return false;
            }
            default: {
                OT_FAIL;
            }
        }
    }())
    , nonce_(nonce)
    , inv_block_([&] {
        using Type = opentxs::blockchain::bitcoin::Inventory::Type;
        // TODO do some chains use MsgWitnessBlock?

        return Type::MsgBlock;
    }())
    , inv_tx_([&] {
        using Type = opentxs::blockchain::bitcoin::Inventory::Type;
        const auto& segwit =
            opentxs::blockchain::params::Chains().at(chain_).segwit_;

        if (segwit) {

            return Type::MsgWitnessTx;
        } else {

            return Type::MsgTx;
        }
    }())
    , protocol_((0 == protocol) ? default_protocol_version_ : protocol)
    , local_services_(get_local_services(protocol_, chain_, config))
    , relay_(true)
    , handshake_()
    , verification_()
{
}

auto Peer::check_handshake() noexcept -> void
{
    if (handshake_.got_version_ && handshake_.got_verack_) {
        transition_state_verify();
    }
}

auto Peer::check_verification() noexcept -> void
{
    const auto verified =
        verification_.got_block_header_ &&
        (verification_.got_cfheader_ || (false == peer_cfilter_));

    if (verified) { transition_state_run(); }
}

auto Peer::commands() noexcept -> const CommandMap&
{
    static const auto map = CommandMap{
        {Command::unknown, &Peer::not_implemented},
        {Command::addr, &Peer::process_protocol_addr},
        {Command::alert, &Peer::not_implemented},
        {Command::block, &Peer::process_protocol_block},
        {Command::blocktxn, &Peer::process_protocol_blocktxn},
        {Command::cfcheckpt, &Peer::process_protocol_cfcheckpt},
        {Command::cfheaders, &Peer::process_protocol_cfheaders},
        {Command::cfilter, &Peer::process_protocol_cfilter},
        {Command::checkorder, &Peer::not_implemented},
        {Command::cmpctblock, &Peer::process_protocol_cmpctblock},
        {Command::feefilter, &Peer::process_protocol_feefilter},
        {Command::filteradd, &Peer::process_protocol_filteradd},
        {Command::filterclear, &Peer::process_protocol_filterclear},
        {Command::filterload, &Peer::process_protocol_filterload},
        {Command::getaddr, &Peer::process_protocol_getaddr},
        {Command::getblocks, &Peer::process_protocol_getblocks},
        {Command::getblocktxn, &Peer::process_protocol_getblocktxn},
        {Command::getcfcheckpt, &Peer::process_protocol_getcfcheckpt},
        {Command::getcfheaders, &Peer::process_protocol_getcfheaders},
        {Command::getcfilters, &Peer::process_protocol_getcfilters},
        {Command::getdata, &Peer::process_protocol_getdata},
        {Command::getheaders, &Peer::process_protocol_getheaders},
        {Command::headers, &Peer::process_protocol_headers},
        {Command::inv, &Peer::process_protocol_inv},
        {Command::mempool, &Peer::process_protocol_mempool},
        {Command::merkleblock, &Peer::process_protocol_merkleblock},
        {Command::notfound, &Peer::process_protocol_notfound},
        {Command::ping, &Peer::process_protocol_ping},
        {Command::pong, &Peer::process_protocol_pong},
        {Command::reject, &Peer::process_protocol_reject},
        {Command::reply, &Peer::not_implemented},
        {Command::sendcmpct, &Peer::process_protocol_sendcmpct},
        {Command::sendheaders, &Peer::process_protocol_sendheaders},
        {Command::submitorder, &Peer::not_implemented},
        {Command::tx, &Peer::process_protocol_tx},
        {Command::verack, &Peer::process_protocol_verack},
        {Command::version, &Peer::process_protocol_version},
    };

    return map;
}

auto Peer::extract_body_size(const zeromq::Frame& header) const noexcept
    -> std::size_t
{
    OT_ASSERT(HeaderType::Size() == header.size());

    try {
        auto raw = HeaderType::BitcoinFormat{header};

        return raw.PayloadSize();
    } catch (...) {

        return 0;
    }
}

auto Peer::get_local_services(
    const opentxs::blockchain::p2p::bitcoin::ProtocolVersion version,
    const opentxs::blockchain::Type network,
    const opentxs::blockchain::node::internal::Config& config) noexcept
    -> UnallocatedSet<opentxs::blockchain::p2p::Service>
{
    using Chain = opentxs::blockchain::Type;
    using Service = opentxs::blockchain::p2p::Service;
    auto output = UnallocatedSet<opentxs::blockchain::p2p::Service>{};

    switch (network) {
        case Chain::Bitcoin:
        case Chain::Bitcoin_testnet3:
        case Chain::Litecoin:
        case Chain::Litecoin_testnet4:
        case Chain::PKT:
        case Chain::PKT_testnet: {
            output.emplace(Service::Witness);
        } break;
        case Chain::BitcoinCash:
        case Chain::BitcoinCash_testnet3:
        case Chain::BitcoinSV:
        case Chain::BitcoinSV_testnet3:
        case Chain::eCash:
        case Chain::eCash_testnet3: {
            output.emplace(Service::BitcoinCash);
        } break;
        case Chain::Unknown:
        case Chain::Ethereum_frontier:
        case Chain::Ethereum_ropsten:
        case Chain::Casper:
        case Chain::Casper_testnet:
        case Chain::UnitTest:
        default: {
        }
    }

    switch (config.profile_) {
        case BlockchainProfile::mobile: {
        } break;
        case BlockchainProfile::desktop:
        case BlockchainProfile::desktop_native: {
            output.emplace(Service::Limited);
            output.emplace(Service::CompactFilters);
        } break;
        case BlockchainProfile::server: {
            output.emplace(Service::Network);
            output.emplace(Service::CompactFilters);
        } break;
        default: {

            OT_FAIL;
        }
    }

    return output;
}

auto Peer::not_implemented(
    std::unique_ptr<HeaderType> pHeader,
    zeromq::Frame&&) noexcept(false) -> void
{
    OT_ASSERT(pHeader);

    const auto& header = *pHeader;
    LogConsole()("Received unimplemented ")(print(header.Command()))(
        " command from ")(name_)
        .Flush();
}

auto Peer::process_broadcastblock(Message&& msg) noexcept -> void
{
    const auto body = msg.Body();

    OT_ASSERT(1 < body.size());

    using Hash = opentxs::blockchain::block::Hash;
    using Inv = opentxs::blockchain::bitcoin::Inventory;
    transmit_protocol_inv(Inv{inv_block_, Hash{body.at(1).Bytes()}});
}

auto Peer::process_broadcasttx(Message&& msg) noexcept -> void
{
    const auto body = msg.Body();

    OT_ASSERT(1 < body.size());

    transmit_protocol_tx(body.at(1).Bytes());
}

auto Peer::process_getblock(Message&& msg) noexcept -> void
{
    const auto body = msg.Body();

    OT_ASSERT(1 < body.size());

    const auto count{body.size() - 1};
    log_(OT_PRETTY_CLASS())(count)(" blocks to request from ")(name_).Flush();
    using Inv = opentxs::blockchain::bitcoin::Inventory;
    using BlockList = UnallocatedVector<Inv>;
    auto blocks = Vector<BlockList>{get_allocator()};
    blocks.reserve(1 + (count / max_inv_));
    blocks.clear();
    blocks.emplace_back();

    for (auto i = 1_uz; i < body.size(); ++i) {
        auto& list = blocks.back();
        list.emplace_back(inv_block_, api_.Factory().Data(body.at(i)));

        if (max_inv_ <= list.size()) { blocks.emplace_back(); }
    }

    for (auto& list : blocks) { transmit_protocol_getdata(std::move(list)); }
}

auto Peer::process_protocol(Message&& message) noexcept -> void
{
    auto body = message.Body();

    try {
        if (3 > body.size()) {
            throw std::runtime_error{"received invalid message from peer"};
        }

        const auto& headerBytes = body.at(1);
        auto& payloadBytes = body.at(2);
        auto pHeader = [&] {
            auto out = std::unique_ptr<HeaderType>{
                factory::BitcoinP2PHeader(api_, chain_, headerBytes)};

            if (false == out.operator bool()) {
                throw std::runtime_error{
                    "received invalid message header from peer"};
            }

            return out;
        }();
        const auto& header = *pHeader;
        const auto command = header.Command();

        if (Command::unknown == command) {
            const auto type =
                UnallocatedCString{headerBytes.Bytes().substr(4, 12)};
            LogError()("Received unhandled ")(type)(" command from ")(name_)
                .Flush();

            return;
        }

        if (const auto chain = header.Network(); chain != chain_) {
            auto error = CString{get_allocator()};
            error.append("received message intended for ");
            error.append(print(chain));

            throw std::runtime_error{error.c_str()};
        }

        const auto checksum =
            opentxs::blockchain::p2p::bitcoin::message::VerifyChecksum(
                api_, header, payloadBytes);

        if (false == checksum) {
            auto error = CString{get_allocator()};
            error.append("invalid checksum for ");
            error.append(print(command));

            throw std::runtime_error{error.c_str()};
        }

        switch (state()) {
            case State::pre_init:
            case State::init:
            case State::connect:
            case State::shutdown: {

                OT_FAIL;
            }
            case State::handshake: {
                switch (command) {
                    case Command::verack:
                    case Command::version: {
                    } break;
                    default: {
                        log_(OT_PRETTY_CLASS())(name_)(": ignoring ")(
                            print(command))(" during handshake")
                            .Flush();
                    }
                };
            } break;
            case State::verify:
            case State::run:
            default: {
                log_(OT_PRETTY_CLASS())(name_)(": received ")(print(command))
                    .Flush();
            }
        }

        const auto& handler = commands().at(command);

        try {
            (this->*handler)(std::move(pHeader), std::move(payloadBytes));
        } catch (const std::exception& e) {
            log_(OT_PRETTY_CLASS())(name_)(": ")(e.what()).Flush();

            return;
        }
    } catch (const std::exception& e) {
        disconnect(e.what());
    }
}

auto Peer::process_protocol_addr(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Addr;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    reset_peers_timer();
    database_.Import([&] {
        using DB = opentxs::blockchain::database::Peer;
        auto peers = UnallocatedVector<DB::Address>{};

        for (const auto& address : message) {
            auto pAddress = address.clone_internal();

            OT_ASSERT(pAddress);

            pAddress->SetLastConnected({});
            peers.emplace_back(std::move(pAddress));
        }

        return peers;
    }());
}

auto Peer::process_protocol_block(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    if (update_block_job(payload.Bytes())) {

        return;
    } else {
        using Task = opentxs::blockchain::node::ManagerJobs;
        network_.Submit([&] {
            auto work = MakeWork(Task::SubmitBlock);
            work.AddFrame(payload);

            return work;
        }());
    }
}

auto Peer::process_protocol_blocktxn(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Blocktxn;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_cfcheckpt(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Cfcheckpt;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_cfheaders(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    OT_ASSERT(header);

    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Cfheaders;
    auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    auto& message = *pMessage;

    switch (state()) {
        case State::verify: {
            process_protocol_cfheaders_verify(message);
        } break;
        case State::run: {
            process_protocol_cfheaders_run(message);
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto Peer::process_protocol_cfheaders_verify(
    opentxs::blockchain::p2p::bitcoin::message::internal::Cfheaders&
        message) noexcept(false) -> void
{
    log_(OT_PRETTY_CLASS())("Received checkpoint cfheader message from ")(name_)
        .Flush();
    auto postcondition = ScopeGuard{[this] {
        if (false == verification_.got_cfheader_) {
            auto why = CString{get_allocator()};
            why.append("Disconnecting "sv);
            why.append(name_);
            why.append(" due to cfheader checkpoint failure"sv);
            disconnect(why);
        }
    }};

    if (const auto count = message.size(); 1u != count) {
        log_(OT_PRETTY_CLASS())(name_)(": unexpected cfheader count: ")(count)
            .Flush();

        return;
    }

    const auto [height, checkpointHash, parentHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    const auto receivedCfheader =
        opentxs::blockchain::internal::FilterHashToHeader(
            api_, message.at(0).Bytes(), message.Previous().Bytes());

    if (filterHash != receivedCfheader) {
        log_(OT_PRETTY_CLASS())(name_)(": unexpected cfheader: ")
            .asHex(receivedCfheader)(". Expected: ")
            .asHex(filterHash)
            .Flush();

        return;
    }

    log_(OT_PRETTY_CLASS())("Cfheader checkpoint validated for ")(name_)
        .Flush();
    verification_.got_cfheader_ = true;
    set_cfilter_capability(true);
    check_verification();
}

auto Peer::process_protocol_cfheaders_run(
    opentxs::blockchain::p2p::bitcoin::message::internal::Cfheaders&
        message) noexcept(false) -> void
{
    try {
        const auto hashCount = message.size();
        auto headers = [&] {
            const auto stop = header_oracle_.LoadHeader(message.Stop());

            if (false == stop.operator bool()) {
                throw std::runtime_error("Stop block not found");
            }

            if (0 > stop->Height()) {
                throw std::runtime_error("Stop block disconnected");
            }

            if (hashCount > static_cast<std::size_t>(stop->Height())) {
                throw std::runtime_error("Too many filter headers returned");
            }

            const auto startHeight =
                stop->Height() -
                static_cast<opentxs::blockchain::block::Height>(hashCount) + 1;
            const auto start =
                header_oracle_.LoadHeader(header_oracle_.BestHash(startHeight));

            if (false == bool(start)) {
                throw std::runtime_error("Start block not found");
            }

            auto output =
                header_oracle_.Ancestors(start->Position(), stop->Position());

            while (output.size() > hashCount) { output.erase(output.begin()); }

            return output;
        }();

        if (headers.size() != hashCount) {
            throw std::runtime_error(
                "Failed to load all block positions for cfheader message");
        }

        auto block = headers.begin();
        auto cfilterHash = message.begin();
        const auto type = message.Type();

        for (; cfilterHash != message.end(); ++block, ++cfilterHash) {
            // TODO cfheaders message should have non-const begin() and end()
            auto& hash =
                const_cast<opentxs::blockchain::cfilter::Hash&>(*cfilterHash);
            update_cfheader_job(type, std::move(*block), std::move(hash));
        }
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(name_)(": ")(e.what()).Flush();
        finish_job();
    }
}

auto Peer::process_protocol_cfilter(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Cfilter;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;

    try {
        const auto block = header_oracle_.LoadHeader(message.Hash());

        if (false == block.operator bool()) {
            throw std::runtime_error("Failed to load block header");
        }

        const auto& blockHash = message.Hash();
        auto cfilter = factory::GCS(
            api_,
            message.Bits(),
            message.FPRate(),
            opentxs::blockchain::internal::BlockHashToFilterKey(
                blockHash.Bytes()),
            message.ElementCount(),
            message.Filter(),
            get_allocator());

        if (false == cfilter.IsValid()) {
            throw std::runtime_error("Failed to instantiate cfilter");
        }

        update_cfilter_job(
            message.Type(), {block->Position()}, std::move(cfilter));
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(name_)(": ")(e.what()).Flush();
        finish_job();
    }
}

auto Peer::process_protocol_cmpctblock(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::Cmpctblock;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_feefilter(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::Feefilter;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_filteradd(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Filteradd;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_filterclear(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Filterclear;
    const auto pMessage = instantiate<Type>(std::move(header));
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_filterload(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Filterload;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_getaddr(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Getaddr;
    const auto pMessage = instantiate<Type>(std::move(header));
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_getblocks(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::Getblocks;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_getblocktxn(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::Getblocktxn;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_getcfcheckpt(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Getcfcheckpt;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_getcfheaders(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Getcfheaders;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    const auto& stop = message.Stop();

    if (false == header_oracle_.IsInBestChain(stop)) { return; }

    const auto fromGenesis = (0 == message.Start());
    const auto blocks = header_oracle_.BestHashes(
        fromGenesis ? 0 : message.Start() - 1, stop, fromGenesis ? 2000 : 2001);

    if (0 == blocks.size()) { return; }

    const auto filterType = message.Type();
    const auto previousHeader =
        filter_oracle_.LoadFilterHeader(filterType, *blocks.cbegin());

    if (previousHeader.empty()) { return; }

    auto filterHashes =
        Vector<opentxs::blockchain::cfilter::Hash>{get_allocator()};
    const auto start = fromGenesis ? 0_uz : 1_uz;
    static const auto blank = opentxs::blockchain::cfilter::Header{};
    const auto& previous = fromGenesis ? blank : previousHeader;

    for (auto i{start}; i < blocks.size(); ++i) {
        const auto& blockHash = blocks.at(i);
        const auto cfilter =
            filter_oracle_.LoadFilter(filterType, blockHash, get_allocator());

        if (false == cfilter.IsValid()) { break; }

        filterHashes.emplace_back(cfilter.Hash());
    }

    if (0 == filterHashes.size()) { return; }

    transmit_protocol_cfheaders(
        filterType, stop, previous, std::move(filterHashes));
}

auto Peer::process_protocol_getcfilters(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Getcfilters;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    const auto& stopHash = message.Stop();
    const auto pStopHeader = header_oracle_.LoadHeader(stopHash);

    if (false == pStopHeader.operator bool()) {
        log_(OT_PRETTY_CLASS())(name_)(
            ": skipping request with unknown stop header")
            .Flush();

        return;
    }

    const auto& stopHeader = *pStopHeader;
    const auto startHeight{message.Start()};
    const auto stopHeight{stopHeader.Height()};

    if (startHeight > stopHeight) {
        log_(OT_PRETTY_CLASS())(name_)(
            ": skipping request with malformed start height (")(
            startHeight)(") vs stop (")(stopHeight)(")")
            .Flush();

        return;
    }

    if (0 > startHeight) {
        log_(OT_PRETTY_CLASS())(name_)(
            ": skipping request with negative start height (")(startHeight)(")")
            .Flush();

        return;
    }

    constexpr auto limit = 1000_uz;
    const auto count =
        static_cast<std::size_t>((stopHeight - startHeight) + 1u);

    if (count > limit) {
        log_(OT_PRETTY_CLASS())(name_)(
            ": skipping request with excessive filter requests (")(
            count)(") vs allowed (")(limit)(")")
            .Flush();

        return;
    } else {
        log_(OT_PRETTY_CLASS())(name_)(": requests ")(
            count)(" filters from height ")(startHeight)(" to ")(
            stopHeight)(" (")
            .asHex(stopHeader.Hash())(")")
            .Flush();
    }

    const auto type = message.Type();
    const auto hashes = header_oracle_.BestHashes(startHeight, stopHash);
    const auto data = [&] {
        auto out = Vector<opentxs::blockchain::GCS>{get_allocator()};
        out.reserve(count);
        out.clear();

        OT_ASSERT(0u == out.size());

        const auto& filters = network_.FilterOracle();

        for (const auto& hash : hashes) {
            log_(OT_PRETTY_CLASS())(name_)(": loading cfilter for block ")
                .asHex(stopHeader.Hash())
                .Flush();
            const auto& cfilter = out.emplace_back(
                filters.LoadFilter(type, hash, out.get_allocator()));

            if (false == cfilter.IsValid()) { break; }
        }

        return out;
    }();

    if (data.size() != count) {
        LogError()(OT_PRETTY_CLASS())(
            name_)(": failed to load all filters, requested (")(
            count)("), loaded (")(data.size())(")")
            .Flush();

        return;
    }

    OT_ASSERT(data.size() == hashes.size());

    auto h{hashes.begin()};

    for (auto g{data.begin()}; g != data.end(); ++g, ++h) {
        transmit_protocol_cfilter(type, *h, *g);
    }
}

auto Peer::process_protocol_getdata(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Getdata;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    using Inv = opentxs::blockchain::bitcoin::Inventory::Type;
    auto notFound =
        UnallocatedVector<opentxs::blockchain::bitcoin::Inventory>{};

    for (const auto& inv : message) {
        switch (inv.type_) {
            case Inv::MsgWitnessTx:
            case Inv::MsgTx: {
                const auto txid = Txid{inv.hash_.Bytes()};
                auto tx = mempool_.Query(txid.Bytes());

                if (tx) {
                    add_known_tx(txid);
                    const auto bytes = [&] {
                        auto out = Space{};
                        tx->Internal().Serialize(writer(out));

                        return out;
                    }();
                    transmit_protocol_tx(reader(bytes));
                } else {
                    notFound.emplace_back(inv);
                }
            } break;
            case Inv::MsgWitnessBlock:
            case Inv::MsgBlock: {
                auto future = block_oracle_.LoadBitcoin(
                    opentxs::blockchain::block::Hash{inv.hash_.Bytes()});

                if (IsReady(future)) {
                    const auto pBlock = future.get();

                    OT_ASSERT(pBlock);

                    const auto& block = *pBlock;
                    transmit_protocol_block([&] {
                        auto output = api_.Factory().Data();
                        block.Serialize(output.WriteInto());

                        return output;
                    }());
                } else {
                    notFound.emplace_back(inv);
                }
            } break;
            case Inv::None:
            case Inv::MsgFilteredBlock:
            case Inv::MsgCmpctBlock:
            case Inv::MsgFilteredWitnessBlock:
            default: {
                notFound.emplace_back(inv);
            }
        }
    }

    if (0 < notFound.size()) {
        transmit_protocol_notfound(std::move(notFound));
    }
}

auto Peer::process_protocol_getheaders(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Getheaders;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    auto previous = opentxs::blockchain::node::HeaderOracle::Hashes{};
    std::copy(message.begin(), message.end(), std::back_inserter(previous));
    const auto hashes =
        header_oracle_.BestHashes(previous, message.StopHash(), 2000);
    transmit_protocol_headers([&] {
        auto out = UnallocatedVector<
            std::unique_ptr<opentxs::blockchain::bitcoin::block::Header>>{};
        std::transform(
            hashes.begin(),
            hashes.end(),
            std::back_inserter(out),
            [&](const auto& hash) -> auto {
                return header_oracle_.Internal().LoadBitcoinHeader(hash);
            });

        return out;
    }());
}

auto Peer::process_protocol_headers(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Headers;
    auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    auto& message = *pMessage;

    switch (state()) {
        case State::verify: {
            process_protocol_headers_verify(message);
        } break;
        case State::run: {
            process_protocol_headers_run(message);
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto Peer::process_protocol_headers_verify(
    opentxs::blockchain::p2p::bitcoin::message::internal::Headers&
        message) noexcept(false) -> void
{
    log_(OT_PRETTY_CLASS())("Received checkpoint block header message from ")(
        name_)
        .Flush();
    auto postcondition = ScopeGuard{[this] {
        if (false == verification_.got_block_header_) {
            auto why = CString{get_allocator()};
            why.append("Disconnecting "sv);
            why.append(name_);
            why.append(" due to block header checkpoint failure"sv);
            disconnect(why);
        }
    }};

    if (const auto count = message.size(); 1u != count) {
        log_(OT_PRETTY_CLASS())(name_)(": unexpected cfheader count: ")(count)
            .Flush();

        return;
    }

    const auto [height, checkpointHash, parentHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    const auto& receivedBlockHash = message.at(0).Hash();

    if (checkpointHash != receivedBlockHash) {
        log_(OT_PRETTY_CLASS())(name_)(": unexpected block header hash: ")
            .asHex(receivedBlockHash)(". Expected: ")
            .asHex(checkpointHash)
            .Flush();

        return;
    }

    log_(OT_PRETTY_CLASS())("Block header checkpoint validated for ")(name_)
        .Flush();
    verification_.got_block_header_ = true;
    set_block_header_capability(true);
    check_verification();
}

auto Peer::process_protocol_headers_run(
    opentxs::blockchain::p2p::bitcoin::message::internal::Headers&
        message) noexcept(false) -> void
{
    auto future = network_.Track([&] {
        using Task = opentxs::blockchain::node::ManagerJobs;
        auto work = MakeWork(Task::SubmitBlockHeader);

        for (const auto& header : message) {
            header.Serialize(work.AppendBytes(), false);
        }

        return work;
    }());
    using Status = std::future_status;
    constexpr auto limit = 10s;

    if (Status::ready == future.wait_for(limit)) {
        // TODO
    }

    update_get_headers_job();
}

auto Peer::process_protocol_inv(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Inv;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    using Inv = opentxs::blockchain::bitcoin::Inventory;
    using Kind = Inv::Type;
    // TODO allocator
    auto txReceived = Vector<Inv>{};
    auto txToDownload = UnallocatedVector<Inv>{};

    for (const auto& inv : message) {
        const auto& hash = inv.hash_;
        log_(OT_PRETTY_CLASS())(name_)(": received ")(inv.DisplayType())(
            " hash ")
            .asHex(hash)
            .Flush();

        switch (inv.type_) {
            case Kind::MsgBlock:
            case Kind::MsgWitnessBlock: {
                const auto block =
                    opentxs::blockchain::block::Hash{hash.Bytes()};
                const auto header = header_oracle_.LoadHeader(block);

                if (header) {
                    log_(OT_PRETTY_CLASS())(name_)(": header for block ")
                        .asHex(block)(" already downloaded")
                        .Flush();
                } else {
                    log_(OT_PRETTY_CLASS())(name_)(
                        ": downloading header for block ")
                        .asHex(block)
                        .Flush();
                    transmit_protocol_getheaders(block);
                }
            } break;
            case Kind::MsgTx:
            case Kind::MsgWitnessTx: {
                add_known_tx(Txid{hash.Bytes()});
                txReceived.emplace_back(inv);
            } break;
            default: {
            }
        }
    }

    if (0 < txReceived.size()) {
        const auto hashes = [&] {
            auto out = UnallocatedVector<ReadView>{};
            std::transform(
                txReceived.begin(),
                txReceived.end(),
                std::back_inserter(out),
                [&](const auto& in) { return in.hash_.Bytes(); });

            return out;
        }();
        const auto result = mempool_.Submit(hashes);

        OT_ASSERT(txReceived.size() == result.size());

        for (auto i{0u}; i < result.size(); ++i) {
            const auto& download = result.at(i);

            if (download) { txToDownload.emplace_back(txReceived.at(i)); }
        }
    }

    if (0 < txToDownload.size()) {
        transmit_protocol_getdata(std::move(txToDownload));
    }
}

auto Peer::process_protocol_mempool(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Mempool;
    const auto pMessage = instantiate<Type>(std::move(header));
    [[maybe_unused]] const auto& message = *pMessage;
    reconcile_mempool();
}

auto Peer::process_protocol_merkleblock(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::Merkleblock;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_notfound(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Notfound;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_ping(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Ping;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    const auto nonce = message.Nonce();

    if (nonce_ == nonce) {
        disconnect("received ping nonce indicates connection to self");
    } else {
        transmit_protocol_pong(nonce);
    }
}

auto Peer::process_protocol_pong(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Pong;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    const auto nonce = message.Nonce();

    if (nonce_ != nonce) { disconnect("invalid nonce in pong"); }
}

auto Peer::process_protocol_reject(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::Reject;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_sendcmpct(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::Sendcmpct;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_sendheaders(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Sendheaders;
    const auto pMessage = instantiate<Type>(std::move(header));
    [[maybe_unused]] const auto& message = *pMessage;
    // TODO
}

auto Peer::process_protocol_tx(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Tx;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;

    if (auto tx = message.Transaction(); tx) {
        add_known_tx(Txid{tx->ID().Bytes()});
        mempool_.Submit(std::move(tx));
    }
}

auto Peer::process_protocol_verack(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    if (const auto state = this->state(); State::handshake != state) {
        auto error = CString{get_allocator()};
        error.append("received ");
        error.append(print(Command::verack));
        error.append(" during ");
        error.append(print_state(state));
        error.append(" state");
        disconnect(error);
    }

    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Verack;
    const auto pMessage = instantiate<Type>(std::move(header));
    [[maybe_unused]] const auto& message = *pMessage;
    handshake_.got_verack_ = true;
    check_handshake();
}

auto Peer::process_protocol_version(
    std::unique_ptr<HeaderType> header,
    zeromq::Frame&& payload) noexcept(false) -> void
{
    if (const auto state = this->state(); State::handshake != state) {
        auto error = CString{get_allocator()};
        error.append("received ");
        error.append(print(Command::version));
        error.append(" during ");
        error.append(print_state(state));
        error.append(" state");
        disconnect(error);
    }

    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Version;
    const auto pMessage = instantiate<Type>(
        std::move(header), protocol_, payload.data(), payload.size());
    const auto& message = *pMessage;
    network_.UpdateHeight(message.Height());
    protocol_ = std::min(protocol_, message.ProtocolVersion());
    update_address(message.RemoteServices());
    transmit_protocol_verack();

    if (Dir::incoming == dir_) { transmit_protocol_version(); }

    handshake_.got_version_ = true;
    check_handshake();
}

auto Peer::reconcile_mempool() noexcept -> void
{
    // TODO use a monotonic allocator
    const auto local = [this] {
        // TODO Mempool::Dump should return a better type
        const auto in = mempool_.Dump();
        auto out = Set<Txid>{};
        std::transform(
            in.begin(),
            in.end(),
            std::inserter(out, out.end()),
            [](const auto& str) { return Txid{str}; });

        return out;
    }();
    const auto remote = get_known_tx(local.get_allocator());
    const auto missing = [&] {
        auto out = Vector<Txid>{local.get_allocator()};
        out.reserve(std::max(local.size(), remote.size()));
        std::set_difference(
            local.begin(),
            local.end(),
            remote.begin(),
            remote.end(),
            std::back_inserter(out));

        return out;
    }();
    using Inv = opentxs::blockchain::bitcoin::Inventory;
    transmit_protocol_inv([&] {
        auto out = UnallocatedVector<Inv>{};

        for (const auto& hash : missing) { out.emplace_back(inv_tx_, hash); }

        return out;
    }());
}

auto Peer::request_checkpoint_block_header() noexcept -> void
{
    auto [height, checkpointBlockHash, parentBlockHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    transmit_protocol_getheaders(
        std::move(parentBlockHash), checkpointBlockHash);
}

auto Peer::request_checkpoint_cfheader() noexcept -> void
{
    auto [height, checkpointBlockHash, parentBlockHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    transmit_protocol_getcfheaders(height, checkpointBlockHash);
}

auto Peer::transition_state_handshake() noexcept -> void
{
    Imp::transition_state_handshake();

    if (Dir::outgoing == dir_) { transmit_protocol_version(); }
}

auto Peer::transition_state_verify() noexcept -> void
{
    Imp::transition_state_verify();

    if (Dir::incoming == dir_) {
        log_(OT_PRETTY_CLASS())(name_)(
            " is not required to validate checkpoints")
            .Flush();
        transition_state_run();
    } else {
        log_(OT_PRETTY_CLASS())(name_)(" must validate block header ");
        request_checkpoint_block_header();

        if (peer_cfilter_) {
            log_("and cfheader ");
            request_checkpoint_cfheader();
        }

        log_("checkpoints").Flush();
    }
}

auto Peer::transmit_block_hash(opentxs::blockchain::block::Hash&& hash) noexcept
    -> void
{
    using Inv = opentxs::blockchain::bitcoin::Inventory;

    transmit_protocol_inv(Inv{inv_block_, std::move(hash)});
}

auto Peer::transmit_ping() noexcept -> void { transmit_protocol_ping(); }

auto Peer::transmit_protocol_block(const Data& serialized) noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Block;
    transmit_protocol<Type>(serialized);
}

auto Peer::transmit_protocol_cfheaders(
    opentxs::blockchain::cfilter::Type type,
    const opentxs::blockchain::block::Hash& stop,
    const opentxs::blockchain::cfilter::Header& previous,
    Vector<opentxs::blockchain::cfilter::Hash>&& hashes) noexcept -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Cfheaders;
    transmit_protocol<Type>(type, stop, previous, std::move(hashes));
}

auto Peer::transmit_protocol_cfilter(
    opentxs::blockchain::cfilter::Type type,
    const opentxs::blockchain::block::Hash& hash,
    const opentxs::blockchain::GCS& filter) noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Cfilter;
    transmit_protocol<Type>(type, hash, filter);
}

auto Peer::transmit_protocol_getaddr() noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Getaddr;
    transmit_protocol<Type>();
}

auto Peer::transmit_protocol_getcfheaders(
    const opentxs::blockchain::block::Height start,
    const opentxs::blockchain::block::Hash& stop) noexcept -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Getcfheaders;
    transmit_protocol<Type>(filter_oracle_.DefaultType(), start, stop);
}

auto Peer::transmit_protocol_getcfilters(
    const opentxs::blockchain::block::Height start,
    const opentxs::blockchain::block::Hash& stop) noexcept -> void
{
    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Getcfilters;
    transmit_protocol<Type>(filter_oracle_.DefaultType(), start, stop);
}

auto Peer::transmit_protocol_getdata(
    opentxs::blockchain::bitcoin::Inventory&& inv) noexcept -> void
{
    transmit_protocol_getdata([&] {
        auto out = UnallocatedVector<opentxs::blockchain::bitcoin::Inventory>{};
        out.emplace_back(std::move(inv));

        return out;
    }());
}

auto Peer::transmit_protocol_getdata(
    UnallocatedVector<opentxs::blockchain::bitcoin::Inventory>&& inv) noexcept
    -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Getdata;
    transmit_protocol<Type>(std::move(inv));
}

auto Peer::transmit_protocol_getheaders() noexcept -> void
{
    static const auto stop = opentxs::blockchain::block::Hash{};

    transmit_protocol_getheaders(stop);
}

auto Peer::transmit_protocol_getheaders(
    const opentxs::blockchain::block::Hash& stop) noexcept -> void
{
    transmit_protocol_getheaders(header_oracle_.RecentHashes(), stop);
}

auto Peer::transmit_protocol_getheaders(
    opentxs::blockchain::block::Hash&& parent,
    const opentxs::blockchain::block::Hash& stop) noexcept -> void
{
    transmit_protocol_getheaders(
        [&] {
            auto out =
                Vector<opentxs::blockchain::block::Hash>{get_allocator()};
            out.emplace_back(std::move(parent));

            return out;
        }(),
        stop);
}

auto Peer::transmit_protocol_getheaders(
    Vector<opentxs::blockchain::block::Hash>&& history,
    const opentxs::blockchain::block::Hash& stop) noexcept -> void
{
    if ((0u < history.size()) && (history.front() == stop)) { return; }

    using Type =
        opentxs::blockchain::p2p::bitcoin::message::internal::Getheaders;
    transmit_protocol<Type>(protocol_, std::move(history), stop);
}

auto Peer::transmit_protocol_headers(
    UnallocatedVector<
        std::unique_ptr<opentxs::blockchain::bitcoin::block::Header>>&&
        headers) noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Headers;
    transmit_protocol<Type>(std::move(headers));
}

auto Peer::transmit_protocol_inv(
    opentxs::blockchain::bitcoin::Inventory&& inv) noexcept -> void
{
    transmit_protocol_inv([&] {
        auto out = UnallocatedVector<opentxs::blockchain::bitcoin::Inventory>{};
        out.emplace_back(std::move(inv));

        return out;
    }());
}

auto Peer::transmit_protocol_inv(
    UnallocatedVector<opentxs::blockchain::bitcoin::Inventory>&& inv) noexcept
    -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Inv;
    transmit_protocol<Type>(std::move(inv));
}

auto Peer::transmit_protocol_mempool() noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Mempool;
    transmit_protocol<Type>();
}

auto Peer::transmit_protocol_notfound(
    UnallocatedVector<opentxs::blockchain::bitcoin::Inventory>&&
        payload) noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Notfound;
    transmit_protocol<Type>(std::move(payload));
}

auto Peer::transmit_protocol_ping() noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Ping;
    transmit_protocol<Type>(nonce_);
}

auto Peer::transmit_protocol_pong(
    const opentxs::blockchain::p2p::bitcoin::Nonce& nonce) noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Pong;
    transmit_protocol<Type>(nonce);
}

auto Peer::transmit_protocol_tx(ReadView serialized) noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Tx;
    transmit_protocol<Type>(serialized);
}

auto Peer::transmit_protocol_verack() noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Verack;
    transmit_protocol<Type>();
}

auto Peer::transmit_protocol_version() noexcept -> void
{
    using Type = opentxs::blockchain::p2p::bitcoin::message::internal::Version;
    const auto& connection = this->connection();
    const auto local = connection.endpoint_data();
    transmit_protocol<Type>(
        connection.style(),
        protocol_,
        local_services_,
        local.first,
        local.second,
        address().Services(),
        connection.address(),
        connection.port(),
        nonce_,
        user_agent_,
        network_.GetHeight(),
        relay_);
}

auto Peer::transmit_request_block_headers() noexcept -> void
{
    transmit_protocol_getheaders();
}

auto Peer::transmit_request_blocks(
    opentxs::blockchain::node::internal::BlockBatch& job) noexcept -> void
{
    using Inv = opentxs::blockchain::bitcoin::Inventory;
    transmit_protocol_getdata([&] {
        auto out = UnallocatedVector<Inv>{};

        for (const auto& hash : job.Get()) {
            log_(OT_PRETTY_CLASS())("requesting block ").asHex(hash).Flush();
            out.emplace_back(inv_block_, hash);
        }

        return out;
    }());
}

auto Peer::transmit_request_blocks(
    opentxs::blockchain::node::BlockJob& job) noexcept -> void
{
    using Inv = opentxs::blockchain::bitcoin::Inventory;
    transmit_protocol_getdata([&] {
        auto out = UnallocatedVector<Inv>{};

        for (const auto& task : job.data_) {
            const auto& hash = task->position_.hash_;
            log_(OT_PRETTY_CLASS())("requesting block ").asHex(hash).Flush();
            out.emplace_back(inv_block_, hash);
        }

        return out;
    }());
}

auto Peer::transmit_request_cfheaders(
    opentxs::blockchain::node::CfheaderJob& job) noexcept -> void
{
    const auto& data = job.data_;

    OT_ASSERT(0 < data.size());

    transmit_protocol_getcfheaders(
        data.front()->position_.height_, data.back()->position_.hash_);
}

auto Peer::transmit_request_cfilters(
    opentxs::blockchain::node::CfilterJob& job) noexcept -> void
{
    const auto& data = job.data_;

    OT_ASSERT(0 < data.size());

    transmit_protocol_getcfilters(
        data.front()->position_.height_, data.back()->position_.hash_);
}

auto Peer::transmit_request_mempool() noexcept -> void
{
    transmit_protocol_mempool();
}

auto Peer::transmit_request_peers() noexcept -> void
{
    transmit_protocol_getaddr();
}

auto Peer::transmit_txid(const Txid& txid) noexcept -> void
{
    using Inv = opentxs::blockchain::bitcoin::Inventory;
    transmit_protocol_inv(Inv{inv_tx_, txid});
}

Peer::~Peer() = default;
}  // namespace opentxs::network::blockchain::bitcoin
