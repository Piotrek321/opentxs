// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                   // IWYU pragma: associated
#include "1_Internal.hpp"                 // IWYU pragma: associated
#include "api/session/notary/Notary.hpp"  // IWYU pragma: associated

#include <atomic>
#include <chrono>
#include <deque>
#include <exception>
#include <iosfwd>
#include <list>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "api/session/Session.hpp"
#include "api/session/base/Scheduler.hpp"
#include "api/session/base/Storage.hpp"
#include "core/OTStorage.hpp"
#include "core/Shutdown.hpp"
#include "internal/api/Context.hpp"
#include "internal/api/Legacy.hpp"
#include "internal/api/network/Factory.hpp"
#include "internal/api/session/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/api/Settings.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Notary.hpp"
#include "opentxs/api/session/Wallet.hpp"
#if OT_CASH
#include "opentxs/blind/Mint.hpp"
#endif  // OT_CASH
#include "opentxs/core/AddressType.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/core/Secret.hpp"  // IWYU pragma: keep
#include "opentxs/core/String.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/identifier/Server.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Options.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "opentxs/util/Time.hpp"
#include "server/MessageProcessor.hpp"
#include "server/Server.hpp"
#include "server/ServerSettings.hpp"

#if OT_CASH
#define SERIES_DIVIDER "."
#define PUBLIC_SERIES ".PUBLIC"
#define MAX_MINT_SERIES 10000
#define MINT_EXPIRE_MONTHS 6
#define MINT_VALID_MONTHS 12
#define MINT_GENERATE_DAYS 7
#endif  // OT_CASH

namespace opentxs::factory
{
auto NotarySession(
    const api::Context& parent,
    Flag& running,
    Options&& args,
    const api::Crypto& crypto,
    const api::Settings& config,
    const opentxs::network::zeromq::Context& context,
    const std::string& dataFolder,
    const int instance) -> std::unique_ptr<api::session::Notary>
{
    using ReturnType = api::session::implementation::Notary;

    try {
        auto output = std::make_unique<ReturnType>(
            parent,
            running,
            std::move(args),
            crypto,
            config,
            context,
            dataFolder,
            instance);

        if (output) {
            try {
                output->Init();
            } catch (const std::invalid_argument& e) {
                LogError()("opentxs::factory::")(__func__)(
                    ": There was a problem creating the server. The server "
                    "contract will be deleted. Error: ")(e.what())
                    .Flush();
                const std::string datafolder = output->DataFolder();
                OTDB::EraseValueByKey(
                    *output,
                    datafolder,
                    ".",
                    "NEW_SERVER_CONTRACT.otc",
                    "",
                    "");
                OTDB::EraseValueByKey(
                    *output, datafolder, ".", "notaryServer.xml", "", "");
                OTDB::EraseValueByKey(
                    *output, datafolder, ".", "seed_backup.json", "", "");
                std::rethrow_exception(std::current_exception());
            }
        }

        return std::move(output);
    } catch (const std::exception& e) {
        LogError()("opentxs::factory::")(__func__)(": ")(e.what()).Flush();

        return {};
    }
}
}  // namespace opentxs::factory

namespace opentxs::api::session
{
auto Notary::DefaultMintKeyBytes() noexcept -> std::size_t { return 1536u; }
}  // namespace opentxs::api::session

namespace opentxs::api::session::implementation
{
Notary::Notary(
    const api::Context& parent,
    Flag& running,
    Options&& args,
    const api::Crypto& crypto,
    const api::Settings& config,
    const opentxs::network::zeromq::Context& context,
    const std::string& dataFolder,
    const int instance)
    : Session(
          parent,
          running,
          std::move(args),
          crypto,
          config,
          context,
          dataFolder,
          instance,
          [&](const auto& zmq, const auto& endpoints, auto& config) {
              return factory::NetworkAPI(
                  *this,
                  parent.Asio(),
                  zmq,
                  endpoints,
                  factory::BlockchainNetworkAPINull(),
                  config,
                  true);
          },
          factory::SessionFactoryAPI(*this))
    , reason_(factory_.PasswordPrompt("Notary operation"))
    , server_p_(new opentxs::server::Server(*this, reason_))
    , server_(*server_p_)
    , message_processor_p_(
          new opentxs::server::MessageProcessor(server_, reason_, running_))
    , message_processor_(*message_processor_p_)
#if OT_CASH
    , mint_thread_()
    , mint_lock_()
    , mint_update_lock_()
    , mint_scan_lock_()
    , mints_()
    , mints_to_check_()
    , mint_key_size_(args_.DefaultMintKeyBytes())
#endif  // OT_CASH
{
    wallet_ = factory::WalletAPI(*this);

    OT_ASSERT(wallet_);
    OT_ASSERT(server_p_);
    OT_ASSERT(message_processor_p_);
}

void Notary::Cleanup()
{
    LogDetail()(OT_PRETTY_CLASS())("Shutting down and cleaning up.").Flush();
    shutdown_sender_.Activate();
    message_processor_.cleanup();
    message_processor_p_.reset();
    server_p_.reset();
    Session::cleanup();
}

void Notary::DropIncoming(const int count) const
{
    return message_processor_.DropIncoming(count);
}

void Notary::DropOutgoing(const int count) const
{
    return message_processor_.DropOutgoing(count);
}

#if OT_CASH
void Notary::generate_mint(
    const std::string& serverID,
    const std::string& unitID,
    const std::uint32_t series) const
{
    auto mint = GetPrivateMint(Factory().UnitID(unitID), series);

    if (mint) {
        LogError()(OT_PRETTY_CLASS())("Mint already exists.").Flush();

        return;
    }

    const std::string nymID{NymID().str()};
    const std::string seriesID =
        std::string(SERIES_DIVIDER) + std::to_string(series);
    mint.reset(
        factory_
            .Mint(
                String::Factory(nymID.c_str()), String::Factory(unitID.c_str()))
            .release());

    OT_ASSERT(mint)

    const auto& nym = server_.GetServerNym();
    const auto now = Clock::now();
    const std::chrono::seconds expireInterval(
        std::chrono::hours(MINT_EXPIRE_MONTHS * 30 * 24));
    const std::chrono::seconds validInterval(
        std::chrono::hours(MINT_VALID_MONTHS * 30 * 24));
    const auto expires = now + expireInterval;
    const auto validTo = now + validInterval;

    if (false == verify_mint_directory(serverID)) {
        LogError()(OT_PRETTY_CLASS())("Failed to create mint directory.")
            .Flush();

        return;
    }

    mint->GenerateNewMint(
        *wallet_,
        series,
        now,
        validTo,
        expires,
        Factory().UnitID(unitID),
        Factory().ServerID(serverID),
        nym,
        1,
        10,
        100,
        1000,
        10000,
        100000,
        1000000,
        10000000,
        100000000,
        1000000000,
        mint_key_size_.load(),
        reason_);
    opentxs::Lock mintLock(mint_lock_);

    if (mints_.end() != mints_.find(unitID)) {
        mints_.at(unitID).erase(PUBLIC_SERIES);
    }

    mint->SetSavePrivateKeys(true);
    mint->SignContract(nym, reason_);
    mint->SaveContract();
    mint->SaveMint(seriesID.c_str());
    mint->SetSavePrivateKeys(false);
    mint->ReleaseSignatures();
    mint->SignContract(nym, reason_);
    mint->SaveContract();
    mint->SaveMint(PUBLIC_SERIES);
    mint->SaveMint();
}
#endif  // OT_CASH

auto Notary::GetAdminNym() const -> std::string
{
    auto output = String::Factory();
    bool exists{false};
    const auto success = config_.Check_str(
        String::Factory("permissions"),
        String::Factory("override_nym_id"),
        output,
        exists);

    if (success && exists) { return output->Get(); }

    return {};
}

auto Notary::GetAdminPassword() const -> std::string
{
    auto output = String::Factory();
    bool exists{false};
    const auto success = config_.Check_str(
        String::Factory("permissions"),
        String::Factory("admin_password"),
        output,
        exists);

    if (success && exists) { return output->Get(); }

    return {};
}

#if OT_CASH
auto Notary::GetPrivateMint(
    const identifier::UnitDefinition& unitID,
    std::uint32_t index) const -> std::shared_ptr<blind::Mint>
{
    opentxs::Lock lock(mint_lock_);
    const std::string id{unitID.str()};
    const std::string seriesID =
        std::string(SERIES_DIVIDER) + std::to_string(index);
    auto& seriesMap = mints_[id];
    // Modifying the private version may invalidate the public version
    seriesMap.erase(PUBLIC_SERIES);
    auto& output = seriesMap[seriesID];

    if (false == bool(output)) {
        output = load_private_mint(lock, id, seriesID);
    }

    return output;
}

auto Notary::GetPublicMint(const identifier::UnitDefinition& unitID) const
    -> std::shared_ptr<const blind::Mint>
{
    opentxs::Lock lock(mint_lock_);
    const std::string id{unitID.str()};
    const std::string seriesID{PUBLIC_SERIES};
    auto& output = mints_[id][seriesID];

    if (false == bool(output)) {
        output = load_public_mint(lock, id, seriesID);
    }

    return output;
}
#endif  // OT_CASH

auto Notary::GetUserName() const -> std::string { return args_.NotaryName(); }

auto Notary::GetUserTerms() const -> std::string { return args_.NotaryTerms(); }

auto Notary::ID() const -> const identifier::Server&
{
    return server_.GetServerID();
}

void Notary::Init()
{
#if OT_CASH
    mint_thread_ = std::thread(&Notary::mint, this);
#endif  // OT_CASH

    Scheduler::Start(storage_.get(), network_.DHT());
    Storage::init(factory_, crypto_.Seed());

    Start();
}

#if OT_CASH
auto Notary::last_generated_series(
    const std::string& serverID,
    const std::string& unitID) const -> std::int32_t
{
    std::uint32_t output{0};

    for (output = 0; output < MAX_MINT_SERIES; ++output) {
        const std::string filename =
            unitID + SERIES_DIVIDER + std::to_string(output);
        const auto exists = OTDB::Exists(
            *this,
            data_folder_,
            parent_.Internal().Legacy().Mint(),
            serverID.c_str(),
            filename.c_str(),
            "");

        if (false == exists) { return output - 1; }
    }

    return -1;
}

auto Notary::load_private_mint(
    const opentxs::Lock& lock,
    const std::string& unitID,
    const std::string seriesID) const -> std::shared_ptr<blind::Mint>
{
    OT_ASSERT(verify_lock(lock, mint_lock_));

    std::shared_ptr<blind::Mint> mint{factory_.Mint(
        String::Factory(ID()),
        String::Factory(NymID()),
        String::Factory(unitID.c_str()))};

    OT_ASSERT(mint);

    return verify_mint(lock, unitID, seriesID, mint);
}

auto Notary::load_public_mint(
    const opentxs::Lock& lock,
    const std::string& unitID,
    const std::string seriesID) const -> std::shared_ptr<blind::Mint>
{
    OT_ASSERT(verify_lock(lock, mint_lock_));

    std::shared_ptr<blind::Mint> mint{
        factory_.Mint(String::Factory(ID()), String::Factory(unitID.c_str()))};

    OT_ASSERT(mint);

    return verify_mint(lock, unitID, seriesID, mint);
}
#endif  // OT_CASH

auto Notary::MakeInprocEndpoint() const -> std::string
{
    auto out = std::stringstream{};
    out << "inproc://opentxs/notary/";
    out << std::to_string(instance_);

    return out.str();
}

#if OT_CASH
void Notary::mint() const
{
    opentxs::Lock updateLock(mint_update_lock_, std::defer_lock);

    while (server_.GetServerID().empty()) {
        Sleep(std::chrono::milliseconds(50));
    }

    const std::string serverID{server_.GetServerID().str()};

    OT_ASSERT(false == serverID.empty());

    while (running_) {
        Sleep(std::chrono::milliseconds(250));

        if (false == opentxs::server::ServerSettings::__cmd_get_mint) {
            continue;
        }

        std::string unitID{""};
        updateLock.lock();

        if (0 < mints_to_check_.size()) {
            unitID = mints_to_check_.back();
            mints_to_check_.pop_back();
        }

        updateLock.unlock();

        if (unitID.empty()) { continue; }

        const auto last = last_generated_series(serverID, unitID);
        const auto next = last + 1;

        if (0 > last) {
            generate_mint(serverID, unitID, 0);

            continue;
        }

        auto mint = GetPrivateMint(Factory().UnitID(unitID), last);

        if (false == bool(mint)) {
            LogError()(OT_PRETTY_CLASS())("Failed to load existing series.")
                .Flush();

            continue;
        }

        const auto now = Clock::now();
        const auto expires = mint->GetExpiration();
        const std::chrono::seconds limit(
            std::chrono::hours(24 * MINT_GENERATE_DAYS));
        const bool generate = ((now + limit) > expires);

        if (generate) {
            generate_mint(serverID, unitID, next);
        } else {
            LogDetail()(OT_PRETTY_CLASS())("Existing mint file for ")(
                unitID)(" is still valid.")
                .Flush();
        }
    }
}
#endif  // OT_CASH

auto Notary::NymID() const -> const identifier::Nym&
{
    return server_.GetServerNym().ID();
}

void Notary::ScanMints() const
{
#if OT_CASH
    opentxs::Lock scanLock(mint_scan_lock_);
    opentxs::Lock updateLock(mint_update_lock_, std::defer_lock);
    const auto units = wallet_->UnitDefinitionList();

    for (const auto& it : units) {
        const auto& id = it.first;
        updateLock.lock();
        mints_to_check_.push_front(id);
        updateLock.unlock();
    }
#endif  // OT_CASH
}

void Notary::Start()
{
    server_.Init();
    server_.ActivateCron();
    std::string hostname{};
    std::uint32_t port{0};
    core::AddressType type{core::AddressType::Inproc};
    const auto connectInfo = server_.GetConnectInfo(type, hostname, port);

    OT_ASSERT(connectInfo);

    auto pubkey = Data::Factory();
    auto privateKey = server_.TransportKey(pubkey);
    message_processor_.init(
        (core::AddressType::Inproc == type), port, privateKey);
    message_processor_.Start();
#if OT_CASH
    ScanMints();
#endif  // OT_CASH
}

void Notary::UpdateMint(
    [[maybe_unused]] const identifier::UnitDefinition& unitID) const
{
#if OT_CASH
    opentxs::Lock updateLock(mint_update_lock_);
    mints_to_check_.push_front(unitID.str());
#endif  // OT_CASH
}

auto Notary::verify_lock(const opentxs::Lock& lock, const std::mutex& mutex)
    const -> bool
{
    if (lock.mutex() != &mutex) {
        LogError()(OT_PRETTY_CLASS())("Incorrect mutex.").Flush();

        return false;
    }

    if (false == lock.owns_lock()) {
        LogError()(OT_PRETTY_CLASS())("Lock not owned.").Flush();

        return false;
    }

    return true;
}

#if OT_CASH
auto Notary::verify_mint(
    const opentxs::Lock& lock,
    const std::string& unitID,
    const std::string seriesID,
    std::shared_ptr<blind::Mint>& mint) const -> std::shared_ptr<blind::Mint>
{
    OT_ASSERT(verify_lock(lock, mint_lock_));

    if (false == mint->LoadMint(seriesID.c_str())) {
        UpdateMint(Factory().UnitID(unitID));

        return {};
    }

    if (false == mint->VerifyMint(server_.GetServerNym())) {
        LogError()(OT_PRETTY_CLASS())("Invalid mint for ")(unitID).Flush();

        return {};
    }

    return mint;
}

auto Notary::verify_mint_directory(const std::string& serverID) const -> bool
{
    auto serverDir = String::Factory();
    auto mintDir = String::Factory();
    const auto haveMint = parent_.Internal().Legacy().AppendFolder(
        mintDir,
        String::Factory(data_folder_.c_str()),
        String::Factory(parent_.Internal().Legacy().Mint()));
    const auto haveServer = parent_.Internal().Legacy().AppendFolder(
        serverDir, mintDir, String::Factory(serverID.c_str()));

    OT_ASSERT(haveMint)
    OT_ASSERT(haveServer)

    return parent_.Internal().Legacy().BuildFolderPath(serverDir);
}
#endif  // OT_CASH

Notary::~Notary()
{
    running_.Off();
#if OT_CASH
    if (mint_thread_.joinable()) { mint_thread_.join(); }
#endif  // OT_CASH

    Cleanup();
}
}  // namespace opentxs::api::session::implementation
