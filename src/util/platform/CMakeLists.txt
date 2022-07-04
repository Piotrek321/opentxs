# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

target_sources(opentxs-common PRIVATE "All.cpp")

if(OPENTXS_DEBUG_BUILD)
  target_sources(opentxs-common PRIVATE "Debug.cpp")
else()
  target_sources(opentxs-common PRIVATE "Release.cpp")
endif()

if(WIN32)
  target_sources(opentxs-common PRIVATE "non-Apple.cpp" "Windows.cpp")

  if(OPENTXS_DEBUG_BUILD)
    target_sources(opentxs-common PRIVATE "Windows_debug.cpp")
  endif()

  target_include_directories(
    opentxs-common SYSTEM
    PRIVATE "${opentxs_SOURCE_DIR}/deps/robin-hood/src/include"
  )
  target_link_libraries(opentxs-common PRIVATE unofficial-sodium::sodium)
  target_link_libraries(opentxs PUBLIC unofficial-sodium::sodium)
  target_link_libraries(opentxs PUBLIC PThreads4W::PThreads4W)
elseif(UNIX)
  # Use this file for functions which are the same between all non-Windows
  # platforms
  target_sources(opentxs-common PRIVATE "Posix.cpp")

  if(OPENTXS_DEBUG_BUILD)
    # TODO
  else()
    target_sources(opentxs-common PRIVATE "Posix_release.cpp")
  endif()

  target_link_libraries(opentxs-common PRIVATE unofficial-sodium::sodium)
  target_link_libraries(opentxs PUBLIC unofficial-sodium::sodium)

  if(APPLE)
    target_sources(opentxs-common PRIVATE "Apple.cpp")

    if(OPENTXS_DEBUG_BUILD)
      target_sources(opentxs-common PRIVATE "Apple_debug.cpp")
    endif()

    if(IOS)
      target_sources(opentxs-common PRIVATE "iOS.cpp")
    else()
      target_sources(opentxs-common PRIVATE "MacOS.cpp")
    endif()
  else()
    # Use this file for functions which do not need different behavior on
    # Android.
    target_sources(opentxs-common PRIVATE "non-Apple.cpp" "Unix.cpp")

    if(OPENTXS_DEBUG_BUILD)
      target_sources(opentxs-common PRIVATE "Unix_debug.cpp")
    endif()

    target_include_directories(
      opentxs-common SYSTEM
      PRIVATE "${opentxs_SOURCE_DIR}/deps/robin-hood/src/include"
    )

    if(ANDROID)
      target_sources(opentxs-common PRIVATE "Android.cpp")
    else()
      target_sources(opentxs-common PRIVATE "Linux.cpp")
    endif()
  endif()
endif()

if(NOT ANDROID)
  target_sources(opentxs-common PRIVATE "non-Android.cpp")
endif()

target_link_libraries(opentxs-common PRIVATE "${OT_ZMQ_TARGET}")
target_link_libraries(opentxs PUBLIC "${OT_ZMQ_TARGET}")
