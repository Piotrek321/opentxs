// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/util/BlockchainProfile.hpp"

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <string_view>

#include "opentxs/blockchain/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Types.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace network
{
namespace internal
{
class Blockchain;
}  // namespace internal
}  // namespace network
}  // namespace api

namespace blockchain
{
namespace node
{
class Manager;
}  // namespace node
}  // namespace blockchain
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::api::network
{
/**
 The api::network::Blockchain API is used for accessing blockchain-related
 network functionality.
 */
class OPENTXS_EXPORT Blockchain
{
public:
    struct Imp;

    using Chain = opentxs::blockchain::Type;
    using Endpoints = Vector<CString>;

    auto AddSyncServer(const std::string_view endpoint) const noexcept -> bool;
    auto ConnectedSyncServers() const noexcept -> Endpoints;
    auto DeleteSyncServer(const std::string_view endpoint) const noexcept
        -> bool;
    auto Disable(const Chain type) const noexcept -> bool;
    auto Enable(const Chain type, const std::string_view seednode = "")
        const noexcept -> bool;
    auto EnabledChains(alloc::Default alloc = {}) const noexcept -> Set<Chain>;
    /// throws std::out_of_range if chain has not been started
    auto GetChain(const Chain type) const noexcept(false)
        -> const opentxs::blockchain::node::Manager&;
    auto GetSyncServers(alloc::Default alloc = {}) const noexcept -> Endpoints;
    OPENTXS_NO_EXPORT auto Internal() const noexcept -> internal::Blockchain&;
    auto Profile() const noexcept -> BlockchainProfile;
    auto Start(const Chain type, const std::string_view seednode = "")
        const noexcept -> bool;
    auto StartSyncServer(
        const std::string_view syncEndpoint,
        const std::string_view publicSyncEndpoint,
        const std::string_view updateEndpoint,
        const std::string_view publicUpdateEndpoint) const noexcept -> bool;
    auto Stop(const Chain type) const noexcept -> bool;

    OPENTXS_NO_EXPORT Blockchain(Imp* imp) noexcept;
    Blockchain() = delete;
    Blockchain(const Blockchain&) = delete;
    Blockchain(Blockchain&&) = delete;
    auto operator=(const Blockchain&) -> Blockchain& = delete;
    auto operator=(Blockchain&&) -> Blockchain& = delete;

    OPENTXS_NO_EXPORT ~Blockchain();

private:
    Imp* imp_;
};
}  // namespace opentxs::api::network
