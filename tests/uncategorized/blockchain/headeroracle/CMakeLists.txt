# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_opentx_test(
  ottest-blockchain-headeroracle-add_checkpoint_already_in_best_chain
  Test_add_checkpoint_already_in_best_chain.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-add_checkpoint_disconnected
  Test_add_checkpoint_disconnected.cpp
)
add_opentx_test(ottest-blockchain-headeroracle-basic_reorg Test_basic_reorg.cpp)
add_opentx_test(
  ottest-blockchain-headeroracle-basic_reorg-batch Test_basic_reorg-batch.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-basic_sequence Test_basic_sequence.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-basic_sequence-batch
  Test_basic_sequence-batch.cpp
)
add_opentx_test(ottest-blockchain-headeroracle-bitcoin Test_bitcoin.cpp)
add_opentx_test(
  ottest-blockchain-headeroracle-bitcoin-cash Test_bitcoin_cash.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-checkpoint_prevents_reorg
  Test_checkpoint_prevents_reorg.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-checkpoint_prevents_reorg-batch
  Test_checkpoint_prevents_reorg-batch.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-checkpoint_prevents_update
  Test_checkpoint_prevents_update.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-checkpoint_prevents_update-batch
  Test_checkpoint_prevents_update-batch.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-delete_checkpoint Test_delete_checkpoint.cpp
)

if(NOT ANDROID)
  add_opentx_test(ottest-blockchain-headeroracle-random Test_random.cpp)
endif()

add_opentx_test(
  ottest-blockchain-headeroracle-receive_headers_out_of_order
  Test_receive_headers_out_of_order.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-receive_headers_out_of_order-batch
  Test_receive_headers_out_of_order-batch.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-reorg_to_checkpoint
  Test_reorg_to_checkpoint.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-reorg_to_checkpoint_descendent
  Test_reorg_to_checkpoint_descendent.cpp
)
add_opentx_test(
  ottest-blockchain-headeroracle-test_block_serialization
  Test_test_block_serialization.cpp
)
