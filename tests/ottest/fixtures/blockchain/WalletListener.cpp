// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/WalletListener.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <mutex>
#include <string_view>

#include "internal/util/LogMacros.hpp"
#include "internal/util/Mutex.hpp"

namespace ottest
{
struct WalletListener::Imp {
    const ot::api::Session& api_;
    mutable std::mutex lock_;
    std::promise<Height> promise_;
    Height target_;
    ot::OTZMQListenCallback cb_;
    ot::OTZMQSubscribeSocket socket_;

    Imp(const ot::api::Session& api) noexcept
        : api_(api)
        , lock_()
        , promise_()
        , target_(-1)
        , cb_(ot::network::zeromq::ListenCallback::Factory(
              [&](ot::network::zeromq::Message&& msg) {
                  const auto body = msg.Body();

                  OT_ASSERT(body.size() == 4);

                  auto lock = ot::Lock{lock_};
                  const auto height = body.at(2).as<Height>();

                  if (height == target_) {
                      try {
                          promise_.set_value(height);
                      } catch (...) {
                      }
                  }
              }))
        , socket_(api_.Network().ZeroMQ().SubscribeSocket(cb_))
    {
        OT_ASSERT(
            socket_->Start(api_.Endpoints().BlockchainSyncProgress().data()));
    }
};
}  // namespace ottest

namespace ottest
{
WalletListener::WalletListener(const ot::api::Session& api) noexcept
    : imp_(std::make_unique<Imp>(api))
{
}

auto WalletListener::GetFuture(const Height height) noexcept -> Future
{
    auto lock = ot::Lock{imp_->lock_};
    imp_->target_ = height;

    try {
        imp_->promise_ = {};
    } catch (...) {
    }

    return imp_->promise_.get_future();
}

WalletListener::~WalletListener() = default;
}  // namespace ottest
