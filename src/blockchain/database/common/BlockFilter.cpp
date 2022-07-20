// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/database/common/BlockFilter.hpp"  // IWYU pragma: associated

#include <BlockchainFilterHeader.pb.h>
#include <google/protobuf/arena.h>  // IWYU pragma: keep
#include <array>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "Proto.hpp"
#include "Proto.tpp"
#include "blockchain/database/common/Bulk.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/bitcoin/cfilter/GCS.hpp"
#include "internal/util/BoostPMR.hpp"
#include "internal/util/LogMacros.hpp"
#include "internal/util/TSV.hpp"
#include "opentxs/blockchain/bitcoin/cfilter/GCS.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "util/ByteLiterals.hpp"
#include "util/LMDB.hpp"
#include "util/MappedFileStorage.hpp"

namespace opentxs::blockchain::database::common
{
BlockFilter::BlockFilter(
    const api::Session& api,
    storage::lmdb::LMDB& lmdb,
    Bulk& bulk) noexcept
    : api_(api)
    , lmdb_(lmdb)
    , bulk_(bulk)
{
}

auto BlockFilter::HaveFilter(const cfilter::Type type, const ReadView blockHash)
    const noexcept -> bool
{
    try {
        return lmdb_.Exists(translate_filter(type), blockHash);
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();

        return false;
    }
}

auto BlockFilter::HaveFilterHeader(
    const cfilter::Type type,
    const ReadView blockHash) const noexcept -> bool
{
    try {
        return lmdb_.Exists(translate_header(type), blockHash);
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();

        return false;
    }
}

auto BlockFilter::load_filter_index(
    const cfilter::Type type,
    const ReadView blockHash,
    util::IndexData& out) const noexcept(false) -> void
{
    auto tx = lmdb_.TransactionRO();
    load_filter_index(type, blockHash, tx, out);
}

auto BlockFilter::load_filter_index(
    const cfilter::Type type,
    const ReadView blockHash,
    storage::lmdb::LMDB::Transaction& tx,
    util::IndexData& out) const noexcept(false) -> void
{
    auto cb = [&out](const ReadView in) {
        if (sizeof(out) != in.size()) { return; }

        std::memcpy(static_cast<void*>(&out), in.data(), in.size());
    };
    lmdb_.Load(translate_filter(type), blockHash, cb, tx);

    if (0 == out.size_) { throw std::out_of_range("Cfilter not found"); }
}

auto BlockFilter::LoadFilter(
    const cfilter::Type type,
    const ReadView blockHash,
    alloc::Default alloc) const noexcept -> opentxs::blockchain::GCS
{
    try {
        util::IndexData index{};
        load_filter_index(type, blockHash, index);

        return factory::GCS(
            api_, proto::Factory<proto::GCS>(bulk_.ReadView(index)), alloc);
    } catch (const std::exception& e) {
        LogVerbose()(OT_PRETTY_CLASS())(e.what()).Flush();

        return {alloc};
    }
}

auto BlockFilter::LoadFilters(
    const cfilter::Type type,
    const Vector<block::Hash>& blocks) const noexcept -> Vector<GCS>
{
    auto output = Vector<GCS>{blocks.get_allocator()};
    output.reserve(blocks.size());
    // TODO use a named constant for the cfilter scan batch size.
    constexpr auto allocBytes =
        (1000u * sizeof(util::IndexData)) + sizeof(Vector<util::IndexData>);
    auto buf = std::array<std::byte, allocBytes>{};
    auto alloc = alloc::BoostMonotonic{buf.data(), buf.size()};

    Vector<util::IndexData> indices{&alloc};
    indices.reserve(1000u);
    {
        auto tx = lmdb_.TransactionRO();

        for (const auto& hash : blocks) {
            try {
                auto& index = indices.emplace_back();
                load_filter_index(type, hash.Bytes(), tx, index);
            } catch (const std::exception& e) {
                LogVerbose()(OT_PRETTY_CLASS())(e.what()).Flush();
                indices.pop_back();
                break;
            }
        }
    }
    for (const auto& index : indices) {
        try {
            output.emplace_back(factory::GCS(
                api_,
                proto::Factory<proto::GCS>(bulk_.ReadView(index)),
                blocks.get_allocator()));
        } catch (const std::exception& e) {
            LogVerbose()(OT_PRETTY_CLASS())(e.what()).Flush();

            break;
        }
    }

    return output;
}

auto BlockFilter::LoadFilterHash(
    const cfilter::Type type,
    const ReadView blockHash,
    const AllocateOutput filterHash) const noexcept -> bool
{
    auto output{false};
    auto cb = [&output, &filterHash](const auto in) {
        auto size = in.size();

        if ((nullptr == in.data()) || (0 == size)) { return; }

        auto proto =
            proto::Factory<proto::BlockchainFilterHeader>(in.data(), in.size());
        const auto& field = proto.hash();
        auto bytes = filterHash(field.size());

        if (bytes.valid(field.size())) {
            std::memcpy(bytes, field.data(), bytes);
            output = true;
        }
    };

    try {
        lmdb_.Load(translate_header(type), blockHash, cb);
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
    }

    return output;
}

auto BlockFilter::LoadFilterHeader(
    const cfilter::Type type,
    const ReadView blockHash,
    const AllocateOutput header) const noexcept -> bool
{
    auto output{false};
    auto cb = [&output, &header](const auto in) {
        auto size = in.size();

        if ((nullptr == in.data()) || (0 == size)) { return; }

        auto proto =
            proto::Factory<proto::BlockchainFilterHeader>(in.data(), in.size());
        const auto& field = proto.header();
        auto bytes = header(field.size());

        if (bytes.valid(field.size())) {
            std::memcpy(bytes, field.data(), bytes);
            output = true;
        }
    };

    try {
        lmdb_.Load(translate_header(type), blockHash, cb);
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();
    }

    return output;
}

auto BlockFilter::store(
    const Lock& lock,
    storage::lmdb::LMDB::Transaction& tx,
    const ReadView blockHash,
    const cfilter::Type type,
    const GCS& filter) const noexcept -> bool
{
    try {
        proto::GCS proto{};
        if (!filter.Internal().Serialize(proto))
            throw std::runtime_error{"Failed to serialize gcs"};

        const auto bytes = proto.ByteSizeLong();
        const auto table = translate_filter(type);

        auto index = LoadDBTransaction(lmdb_, table, blockHash);

        auto callback = [&](auto& tx) -> bool {
            const auto result = lmdb_.Store(table, blockHash, tsv(index), tx);

            if (false == result.first) {
                LogError()(OT_PRETTY_CLASS())(
                    "Failed to update index for cfilter header")
                    .Flush();

                return false;
            }

            return true;
        };
        auto view =
            bulk_.WriteView(lock, tx, index, std::move(callback), bytes);

        if (false == view.valid(bytes)) {
            throw std::runtime_error{
                "Failed to get write position for cfilter"};
        }

        return proto::write(proto, preallocated(bytes, view.data()));
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();

        return false;
    }
}

auto BlockFilter::StoreFilterHeaders(
    const cfilter::Type type,
    const Vector<CFHeaderParams>& headers) const noexcept -> bool
{
    auto tx = lmdb_.TransactionRW();

    for (const auto& [block, header, hash] : headers) {
        auto proto = proto::BlockchainFilterHeader();
        proto.set_version(1);
        proto.set_header(header.data(), header.size());
        proto.set_hash(hash.data(), hash.size());
        auto bytes = space(proto.ByteSize());
        proto.SerializeWithCachedSizesToArray(
            reinterpret_cast<std::uint8_t*>(bytes.data()));

        try {
            const auto stored = lmdb_.Store(
                translate_header(type), block.Bytes(), reader(bytes), tx);

            if (false == stored.first) { return false; }
        } catch (const std::exception& e) {
            LogError()(OT_PRETTY_CLASS())(e.what()).Flush();

            return false;
        }
    }

    return tx.Finalize(true);
}

auto BlockFilter::StoreFilters(
    const cfilter::Type type,
    const Vector<CFilterParams>& filters) const noexcept -> bool
{
    auto tx = lmdb_.TransactionRW();
    auto lock = Lock{bulk_.Mutex()};

    for (const auto& [block, cfilter] : filters) {
        OT_ASSERT(cfilter.IsValid());

        if (false == store(lock, tx, block.Bytes(), type, cfilter)) {
            return false;
        }
    }

    return tx.Finalize(true);
}

auto BlockFilter::StoreFilters(
    const cfilter::Type type,
    const Vector<CFHeaderParams>& headers,
    const Vector<CFilterParams>& filters) const noexcept -> bool
{
    try {
        if (headers.size() != filters.size()) {
            throw std::runtime_error{
                "wrong number of filters compared to headers"};
        }

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

        // NOTE do as much work as possible before locking mutexes
        static const auto options = [] {
            auto out = google::protobuf::ArenaOptions{};
            out.start_block_size = 8_mib;
            out.max_block_size = 8_mib;

            return out;
        }();
        auto arena = google::protobuf::Arena{options};
        auto upstream = alloc::StandardToBoost(alloc::System());
        auto alloc = alloc::BoostMonotonic{4_mib, &upstream};
        auto data = [&] {
            auto out = Vector<StorageItem>{&alloc};
            out.reserve(headers.size());
            auto h = headers.cbegin();
            auto f = filters.cbegin();

            for (auto end = filters.cend(); end != f; ++f, ++h) {
                auto& [bHash, cfHeader, cfilter, bytes, index] =
                    out.emplace_back(
                        BlockHash{},
                        SerializedCfheader{&alloc},
                        google::protobuf::Arena::Create<proto::GCS>(&arena),
                        0,
                        BulkIndex{});
                bHash = std::get<0>(*h).Bytes();
                const auto* cfheaderProto = [&] {
                    const auto& [block, header, hash] = *h;
                    auto* proto = google::protobuf::Arena::Create<
                        proto::BlockchainFilterHeader>(&arena);
                    proto->set_version(1);
                    proto->set_header(header.data(), header.size());
                    proto->set_hash(hash.data(), hash.size());

                    return proto;
                }();
                proto::write(*cfheaderProto, writer(cfHeader));

                if (const auto& [b, filter] = *f;
                    (false == filter.IsValid()) ||
                    (false == filter.Internal().Serialize(*cfilter))) {
                    throw std::runtime_error{"Failed to serialize gcs"};
                }

                bytes = cfilter->ByteSizeLong();
            }

            return out;
        }();
        const auto hTable = translate_header(type);
        const auto fTable = translate_filter(type);
        auto tx = lmdb_.TransactionRW();
        auto lock = Lock{bulk_.Mutex()};

        for (auto& i : data) {

            auto writeIndex = [&](auto& tx) -> bool {
                const auto& [block, header, filter, bytes, index] = i;
                const auto result = lmdb_.Store(fTable, block, tsv(index), tx);

                if (false == result.first) {
                    LogError()(OT_PRETTY_CLASS())(
                        "Failed to update index for cfilter header")
                        .Flush();

                    return false;
                }

                return true;
            };
            auto& [block, header, filter, bytes, index] = i;
            LoadDBTransaction(lmdb_, fTable, block);

            auto view =
                bulk_.WriteView(lock, tx, index, std::move(writeIndex), bytes);

            if (false == view.valid(bytes)) {
                throw std::runtime_error{
                    "Failed to get write position for cfilter"};
            }

            if (!proto::write(*filter, preallocated(bytes, view.data()))) {
                throw std::runtime_error{"Failed to get write cfilter"};
            }

            const auto stored = lmdb_.Store(hTable, block, reader(header), tx);

            if (false == stored.first) {
                throw std::runtime_error{"Failed to get write cfheader"};
            }
        }

        lock.unlock();

        return tx.Finalize(true);
    } catch (const std::exception& e) {
        LogError()(OT_PRETTY_CLASS())(e.what()).Flush();

        return false;
    }
}

auto BlockFilter::translate_filter(const cfilter::Type type) noexcept(false)
    -> Table
{
    switch (type) {
        case cfilter::Type::Basic_BIP158: {
            return FilterIndexBasic;
        }
        case cfilter::Type::Basic_BCHVariant: {
            return FilterIndexBCH;
        }
        case cfilter::Type::ES: {
            return FilterIndexES;
        }
        default: {
            throw std::runtime_error("Unsupported filter type");
        }
    }
}

auto BlockFilter::translate_header(const cfilter::Type type) noexcept(false)
    -> Table
{
    switch (type) {
        case cfilter::Type::Basic_BIP158: {
            return FilterHeadersBasic;
        }
        case cfilter::Type::Basic_BCHVariant: {
            return FilterHeadersBCH;
        }
        case cfilter::Type::ES: {
            return FilterHeadersOpentxs;
        }
        default: {
            throw std::runtime_error("Unsupported filter type");
        }
    }
}

Vector<BlockFilter::StorageItem> BlockFilter::load_storage_items(
    const Vector<CFHeaderParams>& headers,
    const Vector<CFilterParams>& filters,
    alloc::BoostMonotonic& alloc,
    google::protobuf::Arena& arena)
{
    Vector<StorageItem> data{&alloc};
    data.reserve(headers.size());
    auto h = headers.cbegin();
    auto f = filters.cbegin();

    for (auto end = filters.cend(); end != f; ++f, ++h) {
        auto& [bHash, cfHeader, cfilter, bytes, index] = data.emplace_back(
            BlockHash{},
            SerializedCfheader{&alloc},
            google::protobuf::Arena::Create<proto::GCS>(&arena),
            0,
            BulkIndex{});
        bHash = std::get<0>(*h).Bytes();

        const auto& [block, header, hash] = *h;
        auto* cfheaderProto =
            google::protobuf::Arena::Create<proto::BlockchainFilterHeader>(
                &arena);
        cfheaderProto->set_version(1);
        cfheaderProto->set_header(header.data(), header.size());
        cfheaderProto->set_hash(hash.data(), hash.size());

        proto::write(*cfheaderProto, writer(cfHeader));

        if (auto& [b, filter] = *f;
            !filter.IsValid() || !filter.Internal().Serialize(*cfilter)) {
            throw std::runtime_error{"Failed to serialize gcs"};
        }
        bytes = cfilter->ByteSizeLong();
    }

    return data;
}

}  // namespace opentxs::blockchain::database::common
