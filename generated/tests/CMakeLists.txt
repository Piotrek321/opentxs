# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(NOT MSVC)
  # NOTE the RLP test vectors can not be easily embedded when the compiler is
  # MSVC due to string literal length restrictions
  file(
    READ
    "${opentxs_SOURCE_DIR}/tests/ottest/data/blockchain/ethereum/RLPTests/rlptest.json"
    RLP_JSON
  )
  configure_file(
    "RLP.cpp.in"
    "${opentxs_BINARY_DIR}/tests/ottest/data/blockchain/RLP_raw.cpp"
    @ONLY
  )
endif()
