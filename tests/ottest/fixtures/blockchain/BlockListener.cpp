// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/BlockListener.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <mutex>
#include <utility>

#include "internal/util/LogMacros.hpp"
#include "internal/util/Mutex.hpp"

namespace ottest
{
struct BlockListener::Imp {
    const ot::api::Session& api_;
    const ot::CString name_;
    mutable std::mutex lock_;
    std::promise<Position> promise_;
    Height target_;
    ot::OTZMQListenCallback cb_;
    ot::OTZMQSubscribeSocket socket_;

    Imp(const ot::api::Session& api, std::string_view name)
        : api_(api)
        , name_(name)
        , lock_()
        , promise_()
        , target_(-1)
        , cb_(ot::network::zeromq::ListenCallback::Factory(
              [&](ot::network::zeromq::Message&& msg) {
                  const auto body = msg.Body();

                  OT_ASSERT(0 < body.size());

                  auto position = [&]() -> Position {
                      switch (body.at(0).as<ot::WorkType>()) {
                          case ot::WorkType::BlockchainNewHeader: {
                              OT_ASSERT(3 < body.size());

                              return {
                                  body.at(3).as<Height>(), body.at(2).Bytes()};
                          }
                          case ot::WorkType::BlockchainReorg: {
                              OT_ASSERT(5 < body.size());

                              return {
                                  body.at(5).as<Height>(), body.at(4).Bytes()};
                          }
                          default: {

                              OT_FAIL;
                          }
                      }
                  }();
                  ot::LogConsole()(name_)(" header oracle updated to ")(
                      position)
                      .Flush();
                  auto lock = ot::Lock{lock_};

                  if (position.height_ == target_) {
                      try {
                          promise_.set_value(std::move(position));
                      } catch (...) {
                      }
                  }
              }))
        , socket_(api_.Network().ZeroMQ().SubscribeSocket(cb_))
    {
        OT_ASSERT(socket_->Start(api_.Endpoints().BlockchainReorg().data()));
    }
};
}  // namespace ottest

namespace ottest
{
BlockListener::BlockListener(
    const ot::api::Session& api,
    std::string_view name) noexcept
    : imp_(std::make_unique<Imp>(api, name))
{
}

auto BlockListener::GetFuture(const Height height) noexcept -> Future
{
    auto lock = ot::Lock{imp_->lock_};
    imp_->target_ = height;

    try {
        imp_->promise_ = {};
    } catch (...) {
    }

    return imp_->promise_.get_future();
}

BlockListener::~BlockListener() = default;
}  // namespace ottest
