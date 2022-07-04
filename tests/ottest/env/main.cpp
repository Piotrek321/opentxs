// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "ottest/Basic.hpp"
#include "ottest/Cli.hpp"
#include "ottest/env/OTTestEnvironment.hpp"

auto main(int argc, char** argv) -> int
{
    const auto& args = ottest::Args(false, argc, argv);
    auto parser = ottest::ArgumentParser{};
    parser.parse(argc, argv);

    if (parser.show_help_) {
        std::cout << parser.help(args) << "\n";

        return 0;
    }

    ::testing::AddGlobalTestEnvironment(new ottest::OTTestEnvironment());
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
