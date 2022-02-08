// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"               // IWYU pragma: associated
#include "1_Internal.hpp"             // IWYU pragma: associated
#include "otx/blind/lucre/Lucre.hpp"  // IWYU pragma: associated

extern "C" {
#include <openssl/bio.h>
#include <openssl/ossl_typ.h>
}

#include <cstdio>

#include "internal/util/LogMacros.hpp"

namespace opentxs::otx::blind
{
class LucreDumper::Imp
{
public:
    auto LogToFile() noexcept -> void
    {
        bio_ = ::BIO_new_file("openssl.dump", "w");

        OT_ASSERT(nullptr != bio_);

        SetDumper(bio_);
    }
    auto LogToScreen() noexcept -> void { SetMonitor(stderr); }

    Imp() noexcept
        : bio_(nullptr)
    {
    }

    ~Imp()
    {
        if (nullptr != bio_) {
            ::BIO_free(bio_);
            bio_ = nullptr;
        }
    }

private:
    ::BIO* bio_;

    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;
};

LucreDumper::LucreDumper()
    : imp_(std::make_unique<Imp>())
{
    if (IsEnabled()) { init(); }
}

auto LucreDumper::log_to_file() noexcept -> void { imp_->LogToFile(); }

auto LucreDumper::log_to_screen() noexcept -> void { imp_->LogToScreen(); }

LucreDumper::~LucreDumper() = default;
}  // namespace opentxs::otx::blind