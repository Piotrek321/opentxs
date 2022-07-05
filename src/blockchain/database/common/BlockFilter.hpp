// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <memory>
#include <GCS.pb.h>

#include "internal/blockchain/crypto/Crypto.hpp"
#include "internal/blockchain/database/common/Common.hpp"
#include "internal/util/BoostPMR.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/FilterType.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/GCS.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "util/LMDB.hpp"
#include "util/MappedFileStorage.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
class Session;
}  // namespace api

namespace blockchain
{
namespace block
{
class Hash;
}  // namespace block

namespace database
{
namespace common
{
class Bulk;
}  // namespace common
}  // namespace database

class GCS;
}  // namespace blockchain

namespace storage
{
namespace lmdb
{
class LMDB;
}  // namespace lmdb
}  // namespace storage

namespace util
{
struct IndexData;
}  // namespace util
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::database::common
{
class BlockFilter
{
public:
    auto HaveFilter(const cfilter::Type type, const ReadView blockHash)
        const noexcept -> bool;
    auto HaveFilterHeader(const cfilter::Type type, const ReadView blockHash)
        const noexcept -> bool;
    auto LoadFilter(
        const cfilter::Type type,
        const ReadView blockHash,
        alloc::Default alloc) const noexcept -> opentxs::blockchain::GCS;
    auto LoadFilters(
        const cfilter::Type type,
        const Vector<block::Hash>& blocks) const noexcept -> Vector<GCS>;
    auto LoadFilterHash(
        const cfilter::Type type,
        const ReadView blockHash,
        const AllocateOutput filterHash) const noexcept -> bool;
    auto LoadFilterHeader(
        const cfilter::Type type,
        const ReadView blockHash,
        const AllocateOutput header) const noexcept -> bool;
    auto StoreFilterHeaders(
        const cfilter::Type type,
        const Vector<CFHeaderParams>& headers) const noexcept -> bool;
    auto StoreFilters(
        const cfilter::Type type,
        const Vector<CFilterParams>& filters) const noexcept -> bool;
    auto StoreFilters(
        const cfilter::Type type,
        const Vector<CFHeaderParams>& headers,
        const Vector<CFilterParams>& filters) const noexcept -> bool;

    BlockFilter(
        const api::Session& api,
        storage::lmdb::LMDB& lmdb,
        Bulk& bulk) noexcept;

private:
    using BlockHash = ReadView;
    using SerializedCfheader = Vector<std::byte>;
    using SerializedCfilter = proto::GCS*;
    using CFilterSize = std::size_t;
    using BulkIndex = util::IndexData;
    using StorageItem = std::tuple<
        BlockHash,
        SerializedCfheader,
        SerializedCfilter,
        CFilterSize,
        BulkIndex>;

    const api::Session& api_;
    storage::lmdb::LMDB& lmdb_;
    Bulk& bulk_;

    static auto translate_filter(const cfilter::Type type) noexcept(false)
        -> Table;
    static auto translate_header(const cfilter::Type type) noexcept(false)
        -> Table;

    auto load_filter_index(
        const cfilter::Type type,
        const ReadView blockHash,
        util::IndexData& out) const noexcept(false) -> void;
    auto load_filter_index(
        const cfilter::Type type,
        const ReadView blockHash,
        storage::lmdb::LMDB::Transaction& tx,
        util::IndexData& out) const noexcept(false) -> void;
    auto store(
        const Lock& lock,
        storage::lmdb::LMDB::Transaction& tx,
        const ReadView blockHash,
        const cfilter::Type type,
        const GCS& filter) const noexcept -> bool;

    [[nodiscard]] static Vector<StorageItem> load_storage_items(
        const Vector<CFHeaderParams>& headers,
        const Vector<CFilterParams>& filters,
        alloc::BoostMonotonic& alloc,
        google::protobuf::Arena& arena);
};
}  // namespace opentxs::blockchain::database::common
