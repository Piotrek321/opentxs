// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.ko
#pragma once

#include "ottest/fixtures/blockchain/Regtest.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/blockchain/block/Outpoint.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/block/bitcoin/Block.hpp"
#include "opentxs/blockchain/block/bitcoin/Output.hpp"
#include "opentxs/blockchain/block/bitcoin/Transaction.hpp"
#include "opentxs/blockchain/crypto/Account.hpp"
#include "opentxs/blockchain/crypto/AddressStyle.hpp"
#include "opentxs/blockchain/crypto/Element.hpp"
#include "opentxs/blockchain/crypto/HD.hpp"
#include "opentxs/blockchain/crypto/HDProtocol.hpp"
#include "opentxs/blockchain/crypto/Subaccount.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/blockchain/node/TxoState.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "ottest/fixtures/blockchain/RegtestSimple.hpp"
#include "ottest/fixtures/common/Counter.hpp"
#include "ottest/fixtures/ui/AccountActivity.hpp"
#include "ottest/fixtures/ui/AccountList.hpp"
#include "ottest/fixtures/ui/AccountTree.hpp"
#include "ottest/fixtures/ui/ContactList.hpp"

namespace ottest
{
class Restart_fixture : public Regtest_fixture_simple
{

public:
    int instance = 4;

    using Output = std::pair<
        ot::blockchain::block::Outpoint,
        std::unique_ptr<ot::blockchain::block::bitcoin::Output>>;
    using Outputs = std::pmr::vector<Output>;
    using OutputsSet = std::set<Output>;

    using OutputType = ot::blockchain::node::TxoState;

    std::set<opentxs::blockchain::block::pTxid> bob_unique_transactions{};
    std::set<opentxs::blockchain::block::pTxid> alice_unique_transactions{};

    const std::string name_alice = "Alice";
    const std::string name_bob = "Bob";
    Height targetHeight = 0;
    const int blocks_number_ = 1;
    const int coin_to_send_ = 100000;
    const int balance_after_mine =
        amount_in_transaction_ * blocks_number_ * transaction_in_block_;

    const std::string words_alice = "worry myself exile unit believe climb "
                                    "pitch theme two truly alter daughter";
    const std::string words_bob = "myself two exile unit believe worry "
                                  "daughter climb pitch theme truly alter";

    const User& createUser(const std::string& name, const std::string& words);

    const User& createSenderAlice();

    const User& createReceiverBob();

    void mineForBothUsers(const User& user1, const User& user2);

    void confirmSendTransaction(
        const User& sender,
        std::set<opentxs::blockchain::block::pTxid>& confirmedTransactions);

    void sendCoins(const User& receiver, const User& sender);

    void collectOutputs(
        const User& user_bob,
        const User& user_alice,
        Outputs& all_bobs_outputs,
        Outputs& all_alice_outputs,
        std::map<OutputType, std::size_t>& number_of_oututs_per_type_bob,
        std::map<OutputType, std::size_t>& number_of_oututs_per_type_alice);

    void collectOutputsAsSet(
        const User& user_bob,
        const User& user_alice,
        OutputsSet& all_bobs_outputs,
        OutputsSet& all_alice_outputs,
        std::map<OutputType, std::size_t>& number_of_oututs_per_type_bob,
        std::map<OutputType, std::size_t>& number_of_oututs_per_type_alice);

    std::pair<ot::Amount, ot::Amount> collectFeeRate(
        const User& user_bob,
        const User& user_alice);

    void validateOutputs(
        const User& user_bob_after_reboot,
        const User& user_alice_after_reboot,
        const std::set<Output>& bob_outputs,
        const std::set<Output>& alice_outputs,
        const std::map<OutputType, std::size_t>& bob_all_outputs_size,
        const std::map<OutputType, std::size_t>& alice_all_outputs_size);

    void compareOutputs(
        const std::set<Output>& pre_reboot_outputs,
        const std::set<Output>& post_reboot_outputs);
};
}  // namespace ottest
