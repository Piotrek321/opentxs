// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/Basic.hpp"  // IWYU pragma: associated

#include <boost/filesystem.hpp>
#include <opentxs/opentxs.hpp>
#include <cassert>

namespace fs = boost::filesystem;

namespace ottest
{
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
    static const auto minimal =
        ot::Options{parsed}
            .SetBlockchainProfile(opentxs::BlockchainProfile::desktop_native)
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

auto Home() noexcept -> const ot::UnallocatedCString&
{
    static const auto output = [&] {
        const auto path = fs::temp_directory_path() /
                          fs::unique_path("opentxs-test-%%%%-%%%%-%%%%-%%%%");

        assert(fs::create_directories(path));

        return path.string();
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
