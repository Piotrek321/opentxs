# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(OT_BLOCKCHAIN_EXPORT)
  target_sources(
    opentxs-common
    PRIVATE
      "${opentxs_SOURCE_DIR}/src/internal/network/blockchain/Peer.hpp"
      "${opentxs_SOURCE_DIR}/src/internal/network/blockchain/Types.hpp"
      "HasJob.cpp"
      "HasJob.hpp"
      "Imp.cpp"
      "Imp.hpp"
      "JobType.cpp"
      "JobType.hpp"
      "Peer.cpp"
      "RunJob.cpp"
      "RunJob.hpp"
      "UpdateBlockJob.cpp"
      "UpdateBlockJob.hpp"
      "UpdateCfheaderJob.cpp"
      "UpdateCfheaderJob.hpp"
      "UpdateCfilterJob.cpp"
      "UpdateCfilterJob.hpp"
      "UpdateGetHeadersJob.cpp"
      "UpdateGetHeadersJob.hpp"
  )
  target_link_libraries(opentxs-common PRIVATE Boost::headers)
  target_link_libraries(opentxs PUBLIC Boost::system)
endif()
