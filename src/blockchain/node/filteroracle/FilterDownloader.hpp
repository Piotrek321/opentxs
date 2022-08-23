// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/filteroracle/FilterOracle.hpp"  // IWYU pragma: associated

#include "blockchain/DownloadManager.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/database/Cfilter.hpp"
#include "internal/blockchain/node/HeaderOracle.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/GCS.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/network/zeromq/socket/Socket.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::blockchain::node::implementation
{
using FilterDM = download::Manager<
    FilterOracle::FilterDownloader,
    GCS,
    cfilter::Header,
    cfilter::Type>;
using FilterWorker = Worker<api::Session>;

class FilterOracle::FilterDownloader final : public FilterDM,
                                             public FilterWorker
{
public:
    auto NextBatch() noexcept -> BatchType;
    auto UpdatePosition(const Position& pos) -> void;

    FilterDownloader(
        const api::Session& api,
        database::Cfilter& db,
        const HeaderOracle& header,
        const internal::Manager& node,
        const blockchain::Type chain,
        const cfilter::Type type,
        const UnallocatedCString& shutdown,
        const filteroracle::NotifyCallback& notify) noexcept
        : FilterDM(
              [&] { return db.FilterTip(type); }(),
              [&] {
                  auto promise = std::promise<cfilter::Header>{};
                  const auto tip = db.FilterTip(type);
                  promise.set_value(
                      db.LoadFilterHeader(type, tip.hash_.Bytes()));

                  return Finished{promise.get_future()};
              }(),
              "cfilter",
              20000,
              10000)
        , FilterWorker(api, "FilterDownloader")
        , db_(db)
        , header_(header)
        , node_(node)
        , chain_(chain)
        , type_(type)
        , notify_(notify)
        , last_job_{}
    {
        init_executor({shutdown});
        start();
    }

    ~FilterDownloader() final
    {
        try {
            signal_shutdown().get();
        } catch (const std::exception& e) {
            LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
            // TODO MT-34 improve
        }
    }

private:
    friend FilterDM;

    database::Cfilter& db_;
    const HeaderOracle& header_;
    const internal::Manager& node_;
    const blockchain::Type chain_;
    const cfilter::Type type_;
    const filteroracle::NotifyCallback& notify_;
    FilterOracle::Work last_job_;
   auto process_reset(
        const zmq::Message& in) noexcept -> void;
    auto batch_ready() const noexcept -> void
    {
        node_.JobReady(PeerManagerJobs::JobAvailableCfilters);
    }
    static auto batch_size(std::size_t in) noexcept -> std::size_t
    {
        if (in < 10) {

            return 1;
        } else if (in < 100) {

            return 10;
        } else if (in < 1000) {

            return 100;
        } else {

            return 1000;
        }
    }

        auto pipeline(zmq::Message && in)->void
        {
            if (!running_.load()) { return; }

            const auto body = in.Body();

            OT_ASSERT(0 < body.size());

            using Work = FilterOracle::Work;
            const auto work = body.at(0).as<Work>();
            last_job_ = work;

            switch (work) {
                case Work::shutdown: {
                    protect_shutdown([this] { shut_down(); });
                } break;
                case Work::reset_filter_tip: {
                    process_reset(in);
                } break;
                case Work::heartbeat: {
                    UpdatePosition(db_.FilterHeaderTip(type_));
                    run_if_enabled();
                } break;
                case Work::statemachine: {
                    run_if_enabled();
                } break;
                default: {
                    OT_FAIL;
                }
            }
        }

        auto state_machine() noexcept->int
        {
            tdiag("FilterDownloader::state_machine");
            return FilterDM::state_machine() ? 20 : 400;
        }

        auto shut_down() noexcept->void
        {
            close_pipeline();
            // TODO MT-34 investigate what other actions might be needed
        }

        auto last_job_str()
            const noexcept->std::string
        {
            return FilterOracle::to_str(last_job_);
        }
    };

}// namespace opentxs::blockchain::node::implementation
