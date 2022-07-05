# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_subdirectory(basket)
add_subdirectory(cron)
add_subdirectory(crypto)
add_subdirectory(recurring)
add_subdirectory(script)
add_subdirectory(trade)
add_subdirectory(transaction)
add_subdirectory(util)

target_sources(
  opentxs-common
  PRIVATE
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Account.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/AccountVisitor.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Cheque.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Contract.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Helpers.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Instrument.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Item.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Ledger.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/Message.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/NumList.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/NymFile.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/OTTrackable.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/OTTransaction.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/OTTransactionType.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/StringXML.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/otx/common/XML.hpp"
    "Account.cpp"
    "AccountVisitor.cpp"
    "Cheque.cpp"
    "Contract.cpp"
    "Helpers.cpp"
    "Instrument.cpp"
    "Item.cpp"
    "Ledger.cpp"
    "Message.cpp"
    "NumList.cpp"
    "NymFile.cpp"
    "NymFile.hpp"
    "OTStorage.cpp"
    "OTStorage.hpp"
    "OTTrackable.cpp"
    "OTTransaction.cpp"
    "OTTransactionType.cpp"
    "StringXML.cpp"
    "StringXML.hpp"
    "XML.cpp"
)
