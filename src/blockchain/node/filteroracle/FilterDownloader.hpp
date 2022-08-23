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
        const filteroracle::NotifyCallback& notify) noexcept;

    ~FilterDownloader() final;

private:
    friend FilterDM;

    database::Cfilter& db_;
    const HeaderOracle& header_;
    const internal::Manager& node_;
    const blockchain::Type chain_;
    const cfilter::Type type_;
    const filteroracle::NotifyCallback& notify_;
    FilterOracle::Work last_job_;

    auto process_reset(const zmq::Message& in) noexcept -> void;
    auto batch_ready() const noexcept -> void;
    static auto batch_size(std::size_t in) noexcept -> std::size_t;
    auto check_task(TaskType&) const noexcept -> void;
    auto trigger_state_machine() const noexcept -> void;
    auto update_tip(const Position& position, const cfilter::Header&)
        const noexcept -> void;
    auto queue_processing(DownloadedData&& data) noexcept -> void;

    auto pipeline(zmq::Message&& in) -> void override;
    auto state_machine() noexcept -> int override;
    auto shut_down() noexcept -> void;
    auto last_job_str() const noexcept -> std::string override;
};

}  // namespace opentxs::blockchain::node::implementation
