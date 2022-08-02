// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/Basic.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <cassert>
#include <filesystem>
#include <functional>

#include "internal/util/P0330.hpp"
#include "util/Sodium.hpp"

namespace ottest
{
using namespace opentxs::literals;

auto Args(bool lowlevel, int argc, char** argv) noexcept -> const ot::Options&
{
    using Connection = opentxs::ConnectionMode;
    static const auto parsed = [&] {
        if ((0 < argc) && (nullptr != argv)) {

            return ot::Options{argc, argv};
        } else {

            return ot::Options{};
        }
    }();
    static const auto minimal = ot::Options{parsed}
                                    .SetDefaultMintKeyBytes(288)
                                    .SetHome(Home().c_str())
                                    .SetIpv4ConnectionMode(Connection::off)
                                    .SetIpv6ConnectionMode(Connection::off)
                                    .SetNotaryInproc(true)
                                    .SetTestMode(true);
    static const auto full = ot::Options{minimal}.SetStoragePlugin("mem");

    if (lowlevel) {

        return minimal;
    } else {

        return full;
    }
}

auto Home() noexcept -> const fs::path&
{
    static const auto output = [&]() -> fs::path {
        const auto random = [&] {
            auto buf = ot::Space{};

            assert(opentxs::crypto::sodium::Randomize(ot::writer(buf)(16_uz)));

            return ot::to_hex(buf.data(), buf.size());
        }();

        const auto dir = fs::temp_directory_path() / "ottest" / random;

        assert(fs::create_directories(dir));

        return dir;
    }();

    return output;
}

auto WipeHome() noexcept -> void
{
    try {
        fs::remove_all(Home());
    } catch (...) {
    }
}
}  // namespace ottest
