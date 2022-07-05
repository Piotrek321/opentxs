# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(UNIX
   AND (NOT APPLE)
   AND (NOT ANDROID)
   AND (NOT OT_VALGRIND)
   AND ("${CMAKE_SIZEOF_VOID_P}"
        EQUAL
        "8")
)
  set(OT_MAPPED_FILE_SIZE "8_tib")
  set(OT_DB_FILE_SIZE "1_tib")
else()
  set(OT_MAPPED_FILE_SIZE "1_gib")

  if("${CMAKE_SIZEOF_VOID_P}"
     EQUAL
     "8"
  )
    set(OT_DB_FILE_SIZE "4_gib")
  else()
    set(OT_DB_FILE_SIZE "512_mib")
  endif()
endif()

configure_file(
  "FileSize.hpp.in"
  "${opentxs_BINARY_DIR}/src/util/FileSize.hpp"
  @ONLY
)
