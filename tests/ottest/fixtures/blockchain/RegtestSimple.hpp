// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <opentxs/opentxs.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <utility>

#include "Regtest.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
namespace session
{
class Client;
}  // namespace session

class Session;
}  // namespace api

namespace blockchain
{
namespace block
{
namespace bitcoin
{
class Header;
}  // namespace bitcoin
}  // namespace block

namespace crypto
{
class HD;
}  // namespace crypto

namespace p2p
{
class Address;
}  // namespace p2p
}  // namespace blockchain

class Options;
}  // namespace opentxs

namespace ottest
{
class User;
}  // namespace ottest
// NOLINTEND(modernize-concat-nested-namespaces)

namespace ottest
{
class User;

struct RegtestListener {
    RegtestListener(const ot::api::session::Client& client);

    std::unique_ptr<BlockListener> block_listener;
    std::unique_ptr<WalletListener> wallet_listener;
};

class Regtest_fixture_simple : public Regtest_fixture_normal
{
protected:
    Regtest_fixture_simple();

    using UserIndex = ot::UnallocatedMap<ot::UnallocatedCString, User>;
    using UserListeners =
        ot::UnallocatedMap<ot::UnallocatedCString, RegtestListener>;

    UserIndex users_;
    UserListeners user_listeners_;
    bool wait_for_handshake_ = true;
    static constexpr auto wait_time_limit_ = std::chrono::minutes(1);
    const unsigned amount_in_transaction_ = 10000000;
    const unsigned transaction_in_block_ = 50;

    auto CreateNym(
        const ot::api::session::Client& api,
        const ot::UnallocatedCString& name,
        const ot::UnallocatedCString& seed,
        int index) noexcept -> const User&;

    auto ImportBip39(
        const ot::api::Session& api,
        const ot::UnallocatedCString& words) const noexcept
        -> ot::UnallocatedCString;

    auto CreateClient(
        ot::Options client_args,
        int instance,
        const ot::UnallocatedCString& name,
        const ot::UnallocatedCString& words,
        const b::p2p::Address& address) -> std::pair<const User&, bool>;

    auto CloseClient(const ot::UnallocatedCString& name) -> void;

    auto MineBlocks(
        const User& user,
        Height ancestor,
        unsigned block_number,
        unsigned transaction_number,
        unsigned amount) noexcept
        -> std::unique_ptr<opentxs::blockchain::block::bitcoin::Header>;

    auto MineBlocks(
        Height ancestor,
        std::size_t block_number,
        const Generator& gen,
        const ot::UnallocatedVector<Transaction>& extra) noexcept
        -> std::unique_ptr<opentxs::blockchain::block::bitcoin::Header>;

    auto MineBlocks(const Height ancestor, const std::size_t count) noexcept
        -> bool;

    auto TransactionGenerator(
        const User& user,
        Height height,
        unsigned count,
        unsigned amount) -> Transaction;

    auto WaitForSynchro(
        const User& user,
        const Height target,
        const Amount expected_balance) -> void;

    auto GetBalance(const User& user) -> const Amount;
    auto GetDisplayBalance(const User& user) -> const ot::UnallocatedCString;
    auto GetSyncProgress(const User& user) -> const std::pair<int, int>;
    auto GetSyncPercentage(const User& user) -> double;

    auto GetNextBlockchainAddress(const User& user)
        -> const ot::UnallocatedCString;

    auto GetHDAccount(const User& user) const noexcept -> const bca::HD&;

    auto GetDisplayBalance(opentxs::Amount value) const noexcept -> std::string;
    auto GetWalletAddress(const User& user) const noexcept -> std::string;
    auto GetWalletName(const User& user) const noexcept -> std::string;
    auto GetTransactions(const User& user) const noexcept
        -> opentxs::UnallocatedVector<opentxs::blockchain::block::pTxid>;
};
}  // namespace ottest
