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
#include "opentxs/api/Context.hpp"
#include "opentxs/api/crypto/Blockchain.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/blockchain/bitcoin/block/Block.hpp"
#include "opentxs/blockchain/bitcoin/block/Output.hpp"
#include "opentxs/blockchain/bitcoin/block/Transaction.hpp"
#include "opentxs/blockchain/block/Outpoint.hpp"
#include "opentxs/blockchain/block/Types.hpp"
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
#include "opentxs/util/Container.hpp"
#include "ottest/fixtures/blockchain/RegtestSimple.hpp"
#include "ottest/fixtures/common/Counter.hpp"
#include "ottest/fixtures/ui/AccountActivity.hpp"
#include "ottest/fixtures/ui/AccountList.hpp"
#include "ottest/fixtures/ui/AccountTree.hpp"
#include "ottest/fixtures/ui/ContactList.hpp"
#include "util/storage/tree/Node.hpp"

namespace ottest
{
using namespace opentxs;
class Restart_fixture : public Regtest_fixture_simple
{

public:
    int instance_ = 3;

    const std::string name_alice_ = "Alice";
    const std::string name_bob_ = "Bob";
    Height current_height_ = 0;
    const int blocks_number_ = 1;
    const int coin_to_send_ = 100000;
    const int balance_after_mine_ =
        amount_in_transaction_ * blocks_number_ * transaction_in_block_;

    const std::string words_alice_ = "name_bob_ "
                                     "pitch theme two truly alter daughter";
    const std::string words_bob_ = "myself two exile unit believe worry "
                                   "daughter climb pitch theme truly alter";

    const User& CreateUser(const std::string& name, const std::string& words);
};
}  // namespace ottest
