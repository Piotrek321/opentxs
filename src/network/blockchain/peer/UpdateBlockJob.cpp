// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "network/blockchain/peer/UpdateBlockJob.hpp"  // IWYU pragma: associated

#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"

namespace opentxs::network::blockchain::internal
{
Peer::Imp::UpdateBlockJob::UpdateBlockJob(Imp& parent, ReadView data) noexcept
    : parent_(parent)
    , data_(data)
{
}

auto Peer::Imp::UpdateBlockJob::operator()(std::monostate& job) noexcept
    -> JobUpdate
{
    return {false, false};
}

auto Peer::Imp::UpdateBlockJob::operator()(GetHeadersJob& job) noexcept
    -> JobUpdate
{
    return {false, false};
}

auto Peer::Imp::UpdateBlockJob::operator()(
    opentxs::blockchain::node::internal::BlockBatch& job) noexcept -> JobUpdate
{
    job.Submit(data_);

    return {true, (0u == job.Remaining())};
}

auto Peer::Imp::UpdateBlockJob::operator()(
    opentxs::blockchain::node::CfheaderJob& job) noexcept -> JobUpdate
{
    return {false, false};
}

auto Peer::Imp::UpdateBlockJob::operator()(
    opentxs::blockchain::node::CfilterJob& job) noexcept -> JobUpdate
{
    return {false, false};
}

auto Peer::Imp::UpdateBlockJob::operator()(
    opentxs::blockchain::node::BlockJob& job) noexcept -> JobUpdate
{
    try {
        if (false == valid(data_)) {
            throw std::runtime_error("Invalid payload");
        }

        auto pBlock =
            parent_.api_.Factory().BitcoinBlock(parent_.chain_, data_);

        if (false == pBlock.operator bool()) {
            throw std::runtime_error("Failed to instantiate block");
        }

        const auto& block = *pBlock;

        if (false == parent_.block_oracle_.Validate(block)) {
            throw std::runtime_error("Invalid block");
        }

        auto pHeader = parent_.header_oracle_.LoadHeader(block.Header().Hash());

        if (false == pHeader.operator bool()) {
            throw std::runtime_error("Failed to load block header");
        }

        const auto& header = *pHeader;
        const auto rc = job.Download(header.Position(), std::move(pBlock));

        if (rc && job.isDownloaded()) { return {true, true}; }

        return {true, !rc};
    } catch (const std::exception& e) {
        parent_.log_(OT_PRETTY_CLASS())(parent_.name_)(": ")(e.what()).Flush();

        return {true, true};
    }
}
}  // namespace opentxs::network::blockchain::internal
