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
#include "ottest/data/crypto/PaymentCodeV3.hpp"

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
namespace bitcoin
{
namespace block
{
class Header;
}  // namespace block
}  // namespace bitcoin

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
using ot::blockchain::node::TxoState;
using namespace opentxs;

class User;

struct RegtestListener {
    RegtestListener(const ot::api::session::Client& client);

    std::unique_ptr<BlockListener> block_listener;
    std::unique_ptr<WalletListener> wallet_listener;
};

class Regtest_fixture_simple : public Regtest_fixture_normal
{
protected:
    using OutputsSet = std::set<UTXO>;
    Regtest_fixture_simple();

    struct WalletDescription {
        WalletDescription()
            : name_{}
            , words_{}
        {
        }
        WalletDescription(
            const ot::UnallocatedCString& name,
            const ot::UnallocatedCString& words)
            : name_{name}
            , words_{words}
        {
        }

        ot::UnallocatedCString name_;
        ot::UnallocatedCString words_;
    };

    using UserIndex = ot::UnallocatedMap<ot::UnallocatedCString, User>;
    using UserListeners =
        ot::UnallocatedMap<ot::UnallocatedCString, RegtestListener>;

    std::vector<Transaction> send_transactions_;
    UserIndex users_;
    UserListeners user_listeners_;
    bool wait_for_handshake_ = true;
    static constexpr auto wait_time_limit_ = std::chrono::minutes(1);
    const unsigned amount_in_transaction_ = 10000000;
    const unsigned transaction_in_block_ = 50;
    static constexpr auto coins_to_send_ = 100000;
    Height target_height = 0;

    WalletDescription core_wallet_;
    std::map<std::string, WalletDescription> auxiliary_wallets_;

    virtual void SetUp() override;

    auto CreateNym(
        const ot::api::session::Client& api,
        const ot::UnallocatedCString& name,
        const ot::UnallocatedCString& seed,
        int index) noexcept -> User&;

    auto ImportBip39(
        const ot::api::Session& api,
        const ot::UnallocatedCString& words) const noexcept
        -> ot::UnallocatedCString;

    auto CreateClient(
        ot::Options client_args,
        int instance,
        const ot::UnallocatedCString& name,
        const ot::UnallocatedCString& words,
        const b::p2p::Address& address) -> std::pair<User&, bool>;

    auto CloseClient(const ot::UnallocatedCString& name) -> void;

    auto MineBlocks(
        const User& user,
        Height ancestor,
        unsigned block_number,
        unsigned transaction_number,
        unsigned amount) noexcept
        -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Header>;

    auto MineBlocks(
        Height ancestor,
        std::size_t block_number,
        const Generator& gen,
        const ot::UnallocatedVector<Transaction>& extra) noexcept
        -> std::unique_ptr<opentxs::blockchain::bitcoin::block::Header>;

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
        const Amount expected_balance,
        std::chrono::seconds = 0s) -> void;

    auto GetBalance(const User& user) const -> const Amount;
    auto GetDisplayBalance(const User& user) const
        -> const ot::UnallocatedCString;
    auto GetSyncProgress(const User& user) const -> const std::pair<int, int>;
    auto GetSyncPercentage(const User& user) const -> double;

    auto GetNextBlockchainAddress(const User& user)
        -> const ot::UnallocatedCString;

    auto GetHDAccount(const User& user) const noexcept -> const bca::HD&;

    auto GetDisplayBalance(opentxs::Amount value) const noexcept -> std::string;
    auto GetWalletAddress(const User& user) const noexcept -> std::string;
    auto GetWalletName(const User& user) const noexcept -> std::string;
    auto GetTransactions(const User& user) const noexcept
        -> opentxs::UnallocatedVector<opentxs::blockchain::block::pTxid>;

    void MineTransaction(
        const User& user,
        const opentxs::blockchain::block::pTxid& transactions_to_confirm,
        Height& current_height);

    void SendCoins(
        const User& receiver,
        const User& sender,
        Height& current_height,
        const int coins_to_send = coins_to_send_);

    std::vector<
        std::unique_ptr<const opentxs::blockchain::bitcoin::block::Transaction>>
    CollectTransactionsForFeeCalculations(
        const User& user,
        const std::vector<Transaction>& send_transactions,
        const Transactions& all_transactions) const;

    Amount CalculateFee(
        const std::vector<Transaction>& send_transactions,
        std::vector<std::unique_ptr<
            const opentxs::blockchain::bitcoin::block::Transaction>>&
            loaded_transactions) const;

    void CollectOutputs(
        const User& user,
        OutputsSet& all_outputs,
        std::map<TxoState, std::size_t>& number_of_outputs_per_type) const;

    // Check if all output data is valid.
    // outputs_to_compare  and number_of_outputs_per_type_to_compare
    // should be taken from CollectOutputs call.
    // user is different user that you want to compare outputs with.
    void ValidateOutputs(
        const User& user,
        const std::set<UTXO>& outputs_to_compare,
        const std::map<TxoState, std::size_t>&
            number_of_outputs_per_type_to_compare) const;

    void MineBlocksForUsers(
        std::vector<std::reference_wrapper<const User>>& users,
        Height& target_height,
        const int& number_of_blocks_to_mine);

    auto GetHeight(const User& user) const noexcept
        -> opentxs::blockchain::block::Height;

private:
    void compare_outputs(
        const std::set<UTXO>& pre_reboot_outputs,
        const std::set<UTXO>& post_reboot_outputs) const;
    void advance_blockchain(
        std::vector<std::reference_wrapper<const User>>& users,
        Height& target_height);
    auto WaitForOutputs(
        const User& receiver,
        std::vector<Transaction>& transactions,
        const size_t output_size) -> void;
};

class Regtest_fixture_round_robin : public Regtest_fixture_simple
{
public:
    int instance_ = 2;
    virtual void SetUp() override;
};

}  // namespace ottest
