// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/blockchain/BlockchainType.hpp"

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "opentxs/blockchain/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Types.hpp"

class QObject;

namespace opentxs
{
class OPENTXS_EXPORT Options final
{
public:
    auto BlockchainBindIpv4() const noexcept -> const Set<CString>&;
    auto BlockchainBindIpv6() const noexcept -> const Set<CString>&;
    auto BlockchainProfile() const noexcept -> opentxs::BlockchainProfile;
    auto BlockchainWalletEnabled() const noexcept -> bool;
    auto DefaultMintKeyBytes() const noexcept -> std::size_t;
    auto DisabledBlockchains() const noexcept -> const Set<blockchain::Type>&;
    auto Experimental() const noexcept -> bool;
    auto HelpText() const noexcept -> std::string_view;
    auto Home() const noexcept -> std::filesystem::path;
    auto Ipv4ConnectionMode() const noexcept -> ConnectionMode;
    auto Ipv6ConnectionMode() const noexcept -> ConnectionMode;
    auto LogLevel() const noexcept -> int;
    auto NotaryBindIP() const noexcept -> std::string_view;
    auto NotaryBindPort() const noexcept -> std::uint16_t;
    auto NotaryInproc() const noexcept -> bool;
    auto NotaryName() const noexcept -> std::string_view;
    auto NotaryPublicEEP() const noexcept -> const Set<CString>&;
    auto NotaryPublicIPv4() const noexcept -> const Set<CString>&;
    auto NotaryPublicIPv6() const noexcept -> const Set<CString>&;
    auto NotaryPublicOnion() const noexcept -> const Set<CString>&;
    auto NotaryPublicPort() const noexcept -> std::uint16_t;
    auto NotaryTerms() const noexcept -> std::string_view;
    auto ProvideBlockchainSyncServer() const noexcept -> bool;
    auto QtRootObject() const noexcept -> QObject*;
    auto RemoteBlockchainSyncServers() const noexcept -> const Set<CString>&;
    auto RemoteLogEndpoint() const noexcept -> std::string_view;
    auto StoragePrimaryPlugin() const noexcept -> std::string_view;
    auto TestMode() const noexcept -> bool;

    auto AddBlockchainIpv4Bind(std::string_view endpoint) noexcept -> Options&;
    auto AddBlockchainIpv6Bind(std::string_view endpoint) noexcept -> Options&;
    auto AddBlockchainSyncServer(std::string_view endpoint) noexcept
        -> Options&;
    auto AddNotaryPublicEEP(std::string_view value) noexcept -> Options&;
    auto AddNotaryPublicIPv4(std::string_view value) noexcept -> Options&;
    auto AddNotaryPublicIPv6(std::string_view value) noexcept -> Options&;
    auto AddNotaryPublicOnion(std::string_view value) noexcept -> Options&;
    auto DisableBlockchain(blockchain::Type chain) noexcept -> Options&;
    OPENTXS_NO_EXPORT auto ImportOption(
        std::string_view key,
        std::string_view value) noexcept -> Options&;
    auto ParseCommandLine(int argc, char** argv) noexcept -> Options&;
    auto SetBlockchainProfile(opentxs::BlockchainProfile value) noexcept
        -> Options&;
    auto SetBlockchainSyncEnabled(bool enabled) noexcept -> Options&;
    auto SetBlockchainWalletEnabled(bool enabled) noexcept -> Options&;
    auto SetDefaultMintKeyBytes(std::size_t bytes) noexcept -> Options&;
    auto SetExperimental(bool enabled) noexcept -> Options&;
    auto SetHome(const std::filesystem::path& path) noexcept -> Options&;
    auto SetIpv4ConnectionMode(ConnectionMode mode) noexcept -> Options&;
    auto SetIpv6ConnectionMode(ConnectionMode mode) noexcept -> Options&;
    auto SetLogEndpoint(std::string_view endpoint) noexcept -> Options&;
    auto SetLogLevel(int level) noexcept -> Options&;
    auto SetNotaryBindIP(std::string_view value) noexcept -> Options&;
    auto SetNotaryBindPort(std::uint16_t port) noexcept -> Options&;
    auto SetNotaryInproc(bool inproc) noexcept -> Options&;
    auto SetNotaryName(std::string_view value) noexcept -> Options&;
    auto SetNotaryPublicPort(std::uint16_t port) noexcept -> Options&;
    auto SetNotaryTerms(std::string_view value) noexcept -> Options&;
    auto SetQtRootObject(QObject*) noexcept -> Options&;
    auto SetStoragePlugin(std::string_view name) noexcept -> Options&;
    auto SetTestMode(bool test) noexcept -> Options&;

    Options() noexcept;
    Options(int argc, char** argv) noexcept;
    Options(const Options& rhs) noexcept;
    Options(Options&& rhs) noexcept;
    auto operator=(const Options&) -> Options& = delete;
    auto operator=(Options&&) noexcept -> Options& = delete;

    ~Options();

private:
    friend auto operator+(const Options&, const Options&) noexcept -> Options;

    struct Imp;

    Imp* imp_;
};

auto operator+(const Options& lhs, const Options& rhs) noexcept -> Options;
constexpr auto value(ConnectionMode val) noexcept -> int
{
    return static_cast<int>(val);
}
}  // namespace opentxs
