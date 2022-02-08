# Automatically generated by scripts/boost/generate-ports.ps1

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO boostorg/asio
    REF boost-1.77.0
    SHA512 b7387f03994ecb22c545ed162c9622676a806cb7434e29303a72ee91e776034626cc125271439e7fa5983c76c06a887472dc3843e2a8ffca3a6ff3caee763641
    HEAD_REF master
    PATCHES windows_alloca_header.patch pop_options.patch
)

include(${CURRENT_INSTALLED_DIR}/share/boost-vcpkg-helpers/boost-modular-headers.cmake)
boost_modular_headers(SOURCE_PATH ${SOURCE_PATH})