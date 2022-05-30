# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

target_sources(
  opentxs-common
  PRIVATE
    "${opentxs_SOURCE_DIR}/src/internal/blockchain/p2p/P2P.hpp"
    "Address.cpp"
    "Address.hpp"
)
set(cxx-install-headers
    "${opentxs_SOURCE_DIR}/include/opentxs/blockchain/p2p/Address.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/blockchain/p2p/Peer.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/blockchain/p2p/Types.hpp"
)
target_link_libraries(opentxs PUBLIC Boost::system)
target_sources(opentxs-common PRIVATE ${cxx-install-headers})

if(OPENTXS_STANDALONE)
  install(
    FILES ${cxx-install-headers}
    DESTINATION include/opentxs/blockchain/p2p
    COMPONENT dev
  )
endif()
