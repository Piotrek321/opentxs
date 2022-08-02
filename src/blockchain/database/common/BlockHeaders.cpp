// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/database/common/BlockHeaders.hpp"  // IWYU pragma: associated

#include <BlockchainBlockHeader.pb.h>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "Proto.hpp"
#include "Proto.tpp"
#include "blockchain/database/common/Bulk.hpp"
#include "internal/blockchain/block/Header.hpp"
#include "internal/blockchain/database/common/Common.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/TSV.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "util/LMDB.hpp"
#include "util/MappedFileStorage.hpp"

namespace opentxs::blockchain::database::common
{
BlockHeader::BlockHeader(storage::lmdb::LMDB& lmdb, Bulk& bulk) noexcept(false)
    : lmdb_(lmdb)
    , bulk_(bulk)
    , table_(Table::HeaderIndex)
{
}

auto BlockHeader::Exists(
    const opentxs::blockchain::block::Hash& hash) const noexcept -> bool
{
    return lmdb_.Exists(table_, hash.Bytes());
}

auto BlockHeader::Load(const opentxs::blockchain::block::Hash& hash) const
    noexcept(false) -> proto::BlockchainBlockHeader
{
    auto index = LoadDBTransaction(lmdb_, table_, hash.Bytes());

    if (0 == index.size_) { throw std::out_of_range("Block header not found"); }

    return proto::Factory<proto::BlockchainBlockHeader>(bulk_.ReadView(index));
}

auto BlockHeader::Store(
    const opentxs::blockchain::block::Header& header) const noexcept -> bool
{
    auto tx = lmdb_.TransactionRW();
    auto lock = Lock{bulk_.Mutex()};

    if (!store(lock, false, tx, header)) { return false; }

    if (tx.Finalize(true)) { return true; }

    LogError()(OT_PRETTY_CLASS())("Database update error").Flush();

    return false;
}

auto BlockHeader::Store(const UpdatedHeader& headers) const noexcept -> bool
{
    auto tx = lmdb_.TransactionRW();
    auto lock = Lock{bulk_.Mutex()};

    for (const auto& [hash, pair] : headers) {
        const auto& [header, newBlock] = pair;

        if (newBlock) {

            if (!store(lock, true, tx, *header)) { return false; }
        }
    }

    if (tx.Finalize(true)) { return true; }

    LogError()(OT_PRETTY_CLASS())("Database update error").Flush();

    return false;
}

auto BlockHeader::store(
    const Lock& lock,
    bool clearLocal,
    storage::lmdb::LMDB::Transaction& pTx,
    const opentxs::blockchain::block::Header& header) const noexcept -> bool
{
    const auto& hash = header.Hash();

    try {
        block::internal::Header::SerializedType proto{};

        if (!header.Internal().Serialize(proto)) {
            throw std::runtime_error{"Failed to serialized header"};
        }

        if (clearLocal) { proto.clear_local(); }

        const auto bytes = proto.ByteSizeLong();

        util::IndexData index = LoadDBTransaction(lmdb_, table_, hash.Bytes());
        ;

        auto cb2 = [&](auto& tx) -> bool {
            const auto result =
                lmdb_.Store(table_, hash.Bytes(), tsv(index), tx);

            if (false == result.first) {
                LogError()(OT_PRETTY_CLASS())(
                    "Failed to update index for block header ")(hash.asHex())
                    .Flush();

                return false;
            }

            return true;
        };
        auto view = bulk_.WriteView(lock, pTx, index, std::move(cb2), bytes);

        if (!view.valid(bytes)) {
            throw std::runtime_error{
                "Failed to get write position for block header"};
        }

        return proto::write(proto, preallocated(bytes, view.data()));
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();

        return false;
    }
}
}  // namespace opentxs::blockchain::database::common
