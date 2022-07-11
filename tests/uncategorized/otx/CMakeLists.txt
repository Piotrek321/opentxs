# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_subdirectory(broken)

add_opentx_test(ottest-otx Test_Basic.cpp)
add_opentx_test(ottest-otx-messages Test_Messages.cpp)

set_tests_properties(ottest-otx PROPERTIES DISABLED TRUE)
