// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <filesystem>
#include <memory>

#include "opentxs/api/network/Network.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace crypto
{
class Blockchain;
}  // namespace crypto

class Legacy;
class Session;
}  // namespace api

class Options;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::api::network::internal
{
class Network : virtual public network::Network
{
public:
    auto Internal() const noexcept -> const internal::Network& final
    {
        return *this;
    }

    auto Internal() noexcept -> internal::Network& final { return *this; }
    virtual auto Shutdown() noexcept -> void = 0;
    virtual auto Start(
        std::shared_ptr<const api::Session> api,
        const api::crypto::Blockchain& crypto,
        const api::Legacy& legacy,
        const std::filesystem::path& dataFolder,
        const Options& args) noexcept -> void = 0;

    ~Network() override = default;
};
}  // namespace opentxs::api::network::internal
