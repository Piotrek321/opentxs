// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"             // IWYU pragma: associated
#include "1_Internal.hpp"           // IWYU pragma: associated
#include "api/context/Context.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>  // IWYU pragma: keep
#include <iosfwd>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include "2_Factory.hpp"
#include "internal/api/Crypto.hpp"
#include "internal/api/Factory.hpp"
#include "internal/api/Log.hpp"
#include "internal/api/crypto/Factory.hpp"
#include "internal/api/session/Client.hpp"
#include "internal/api/session/Factory.hpp"
#include "internal/api/session/Session.hpp"
#include "internal/interface/rpc/RPC.hpp"
#include "internal/network/zeromq/Factory.hpp"
#include "internal/util/Flag.hpp"
#include "internal/util/Log.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/P0330.hpp"
#include "internal/util/Signals.hpp"
#include "opentxs/OT.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/crypto/Config.hpp"
#include "opentxs/api/crypto/Encode.hpp"
#include "opentxs/api/crypto/Seed.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/crypto/Language.hpp"
#include "opentxs/crypto/SeedStyle.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Options.hpp"
#include "opentxs/util/PasswordCallback.hpp"
#include "opentxs/util/PasswordCaller.hpp"
#include "opentxs/util/Pimpl.hpp"

namespace opentxs::factory
{
auto Context(
    Flag& running,
    const Options& args,
    PasswordCaller* externalPasswordCallback) noexcept
    -> std::unique_ptr<api::internal::Context>
{
    using ReturnType = api::imp::Context;

    return std::make_unique<ReturnType>(
        running, args, externalPasswordCallback);
}
}  // namespace opentxs::factory

namespace opentxs::api
{
auto Context::PrepareSignalHandling() noexcept -> void { Signals::Block(); }

auto Context::SuggestFolder(std::string_view appName) noexcept
    -> std::filesystem::path
{
    return Legacy::SuggestFolder(appName);
}
}  // namespace opentxs::api

namespace opentxs::api::imp
{
auto Context::Sessions::clear(ShutdownFutures& futures) noexcept -> void
{
    shutdown_ = true;
    futures.reserve(server_.size() + client_.size());

    for (auto& session : server_) {
        futures.emplace_back(session->Internal().Stop());
    }

    for (auto& session : client_) {
        futures.emplace_back(session->Internal().Stop());
    }

    server_.clear();
    client_.clear();
}
}  // namespace opentxs::api::imp

namespace opentxs::api::imp
{
Context::Context(Flag& running, const Options& args, PasswordCaller* password)
    : api::internal::Context()
    , Periodic(running)
    , args_(args)
    , home_(args_.Home().c_str())
    , null_callback_(opentxs::Factory::NullCallback())
    , default_external_password_callback_([&] {
        auto out = std::make_unique<PasswordCaller>();

        assert(out);

        out->SetCallback(null_callback_.get());

        return out;
    }())
    , external_password_callback_([&] {
        if (nullptr == password) {

            return default_external_password_callback_.get();
        } else {

            return password;
        }
    }())
    , profile_id_()
    , zmq_context_(opentxs::factory::ZMQContext())
    , log_(factory::Log(*zmq_context_, args_.RemoteLogEndpoint()))
    , legacy_(factory::Legacy(home_))
    , config_()
    , asio_()
    , crypto_(nullptr)
    , factory_(nullptr)
    , zap_(nullptr)
    , sessions_()
    , rpc_(opentxs::Factory::RPC(*this))
    , file_lock_()
    , signal_handler_()
    , shutdown_()
{
    // NOTE: OT_ASSERT is not available until Init() has been called
    assert(null_callback_);
    assert(default_external_password_callback_);
    assert(zmq_context_);
    assert(legacy_);
    assert(log_);
    assert(nullptr != external_password_callback_);
    assert(external_password_callback_->HaveCallback());
    assert(rpc_);
}

auto Context::client_instance(const int count) -> int
{
    // NOTE: Instance numbers must not collide between clients and servers.
    // Clients use even numbers and servers use odd numbers.
    return (2 * count);
}

auto Context::ClientSession(const int instance) const
    -> const api::session::Client&
{
    const auto& output = sessions_.lock_shared()->client_.at(instance);

    OT_ASSERT(output);

    return *output;
}

auto Context::Config(const std::filesystem::path& path) const noexcept
    -> const api::Settings&
{
    const auto& config = [&]() -> auto&
    {
        auto handle = config_.lock();
        auto& map = *handle;

        if (auto i = map.find(path); map.end() == i) {
            const auto [out, rc] = map.try_emplace(
                path,
                factory::Settings(*legacy_, String::Factory(path.c_str())));

            OT_ASSERT(rc);

            return out->second;
        } else {

            return i->second;
        }
    }
    ();

    OT_ASSERT(config);

    return *config;
}

auto Context::Crypto() const noexcept -> const api::Crypto&
{
    OT_ASSERT(crypto_);

    return *crypto_;
}

auto Context::Factory() const noexcept -> const api::Factory&
{
    OT_ASSERT(factory_);

    return *factory_;
}

auto Context::GetPasswordCaller() const noexcept -> PasswordCaller&
{
    OT_ASSERT(nullptr != external_password_callback_);

    return *external_password_callback_;
}

auto Context::ProfileId() const noexcept -> std::string_view
{
    return profile_id_.get();
}

auto Context::Init() noexcept -> void
{
    Init_Log();
    Init_Asio();
    init_pid();
    Init_Crypto();
    Init_Factory();
    Init_Profile();
    Init_Rlimit();
    Init_CoreDump();
    Init_Zap();
}

auto Context::Init_Asio() -> void
{
    asio_ = std::make_unique<network::Asio>(*zmq_context_);

    OT_ASSERT(asio_);

    asio_->Init();
}

auto Context::Init_Crypto() -> void
{
    crypto_ = factory::CryptoAPI(Config(legacy_->OpentxsConfigFilePath()));

    OT_ASSERT(crypto_);
}

auto Context::Init_Profile() -> void
{
    const auto& config = Config(legacy_->OpentxsConfigFilePath());
    auto profile_id_exists{false};
    auto existing_profile_id{String::Factory()};
    config.Check_str(
        String::Factory("profile"),
        String::Factory("profile_id"),
        existing_profile_id,
        profile_id_exists);

    if (profile_id_exists) {
        profile_id_.set_value(existing_profile_id->Get());
    } else {
        const auto new_profile_id(crypto_->Encode().Nonce(20));
        auto new_or_update{true};
        config.Set_str(
            String::Factory("profile"),
            String::Factory("profile_id"),
            new_profile_id,
            new_or_update);
        profile_id_.set_value(new_profile_id->Get());
    }
}

auto Context::Init_Factory() -> void
{
    factory_ = factory::FactoryAPI(*crypto_);

    OT_ASSERT(factory_);

    crypto_->Internal().Init(factory_);
}

auto Context::Init_Log() -> void
{
    OT_ASSERT(legacy_);

    const auto& config = Config(legacy_->OpentxsConfigFilePath());
    auto notUsed{false};
    auto level = std::int64_t{0};
    const auto value = args_.LogLevel();

    if (-1 > value) {
        config.CheckSet_long(
            String::Factory("logging"),
            String::Factory("log_level"),
            0,
            level,
            notUsed);
    } else {
        config.Set_long(
            String::Factory("logging"),
            String::Factory("log_level"),
            value,
            notUsed);
        level = value;
    }

    opentxs::internal::Log::SetVerbosity(static_cast<int>(level));
}

auto Context::init_pid() const -> void
{
    try {
        const auto path = legacy_->PIDFilePath();
        {
            std::ofstream(path.c_str());
        }

        auto lock = boost::interprocess::file_lock{path.c_str()};

        if (false == lock.try_lock()) {
            throw std::runtime_error(
                "Another process has locked the data directory");
        }

        file_lock_.swap(lock);
    } catch (const std::exception& e) {
        LogConsole()(e.what()).Flush();

        OT_FAIL;
    }
}

auto Context::Init_Zap() -> void
{
    zap_.reset(opentxs::Factory::ZAP(*zmq_context_));

    OT_ASSERT(zap_);
}

auto Context::NotarySession(const int instance) const -> const session::Notary&
{
    const auto& output = sessions_.lock_shared()->server_.at(instance);

    OT_ASSERT(output);

    return *output;
}

auto Context::RPC(const rpc::request::Base& command) const noexcept
    -> std::unique_ptr<rpc::response::Base>
{
    return rpc_->Process(command);
}

auto Context::server_instance(const int count) -> int
{
    // NOTE: Instance numbers must not collide between clients and servers.
    // Clients use even numbers and servers use odd numbers.
    return (2 * count) + 1;
}

auto Context::shutdown() noexcept -> void
{
    running_.Off();
    Periodic::Shutdown();
    signal_handler_.modify([&](auto& data) {
        if (nullptr != data.callback_) {
            auto& callback = *data.callback_;
            callback();
            data.callback_ = nullptr;
        }
    });
    rpc_.reset();
    sessions_.lock()->clear(shutdown_);
    zap_.reset();
    shutdown_qt();
    crypto_.reset();
    legacy_.reset();
    factory_.reset();
    config_.lock()->clear();

    if (asio_) {
        asio_->Shutdown();
        asio_.reset();
    }

    opentxs::internal::Log::Shutdown();
    log_.reset();
}

auto Context::StartClientSession(const Options& args, const int instance) const
    -> const api::session::Client&
{
    auto handle = sessions_.lock();

    OT_ASSERT(false == handle->shutdown_);

    auto& vector = handle->client_;
    const auto existing = vector.size();
    const auto count = std::max<std::size_t>(0_uz, instance);
    const auto effective = std::min(count, existing);

    if (effective == existing) {
        const auto count = vector.size();

        OT_ASSERT(std::numeric_limits<int>::max() > count);

        const auto next = static_cast<int>(count);
        const auto instance = client_instance(next);
        auto& client = vector.emplace_back(factory::ClientSession(
            *this,
            running_,
            args_ + args,
            Config(legacy_->ClientConfigFilePath(next)),
            *crypto_,
            *zmq_context_,
            legacy_->ClientDataFolder(next),
            instance));

        OT_ASSERT(client);

        client->InternalClient().Init();

        return *client;
    } else {
        const auto& output = vector.at(effective);

        OT_ASSERT(output);

        return *output;
    }
}

auto Context::StartClientSession(const int instance) const
    -> const api::session::Client&
{
    static const auto blank = Options{};

    return StartClientSession(blank, instance);
}

auto Context::StartClientSession(
    const Options& args,
    const int instance,
    std::string_view recoverWords,
    std::string_view recoverPassphrase) const -> const api::session::Client&
{
    OT_ASSERT(crypto::HaveHDKeys());

    const auto& client = StartClientSession(args, instance);
    auto reason = client.Factory().PasswordPrompt("Recovering a BIP-39 seed");

    if (0 < recoverWords.size()) {
        auto wordList =
            opentxs::Context().Factory().SecretFromText(recoverWords);
        auto phrase =
            opentxs::Context().Factory().SecretFromText(recoverPassphrase);
        client.Crypto().Seed().ImportSeed(
            wordList,
            phrase,
            opentxs::crypto::SeedStyle::BIP39,
            opentxs::crypto::Language::en,
            reason);
    }

    return client;
}

auto Context::StartNotarySession(const Options& args, const int instance) const
    -> const session::Notary&
{
    auto handle = sessions_.lock();

    OT_ASSERT(false == handle->shutdown_);

    auto& vector = handle->server_;
    const auto existing = vector.size();
    const auto count = std::max<std::size_t>(0_uz, instance);
    const auto effective = std::min(count, existing);

    if (effective == existing) {
        const auto count = vector.size();

        OT_ASSERT(std::numeric_limits<int>::max() > count);

        const auto next = static_cast<int>(count);
        const auto instance = server_instance(next);
        auto& server = vector.emplace_back(factory::NotarySession(
            *this,
            running_,
            args_ + args,
            *crypto_,
            Config(legacy_->ServerConfigFilePath(next)),
            *zmq_context_,
            legacy_->ServerDataFolder(next),
            instance));

        OT_ASSERT(server);

        return *server;
    } else {
        const auto& output = vector.at(effective);

        OT_ASSERT(output);

        return *output;
    }
}

auto Context::StartNotarySession(const int instance) const
    -> const session::Notary&
{
    static const auto blank = Options{};

    return StartNotarySession(blank, instance);
}

auto Context::ZAP() const noexcept -> const api::network::ZAP&
{
    OT_ASSERT(zap_);

    return *zap_;
}

Context::~Context()
{
    shutdown();

    for (auto& future : shutdown_) { future.get(); }
}
}  // namespace opentxs::api::imp
