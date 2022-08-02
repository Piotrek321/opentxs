// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                       // IWYU pragma: associated
#include "1_Internal.hpp"                     // IWYU pragma: associated
#include "util/storage/tree/PeerReplies.hpp"  // IWYU pragma: associated

#include <PeerReply.pb.h>
#include <StorageItemHash.pb.h>
#include <StorageNymList.pb.h>
#include <cstdlib>
#include <iostream>
#include <tuple>
#include <utility>

#include "Proto.hpp"
#include "internal/serialization/protobuf/Check.hpp"
#include "internal/serialization/protobuf/verify/PeerReply.hpp"
#include "internal/serialization/protobuf/verify/StorageNymList.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/storage/Driver.hpp"
#include "util/storage/Plugin.hpp"
#include "util/storage/tree/Node.hpp"

namespace opentxs::storage
{
PeerReplies::PeerReplies(const Driver& storage, const UnallocatedCString& hash)
    : Node(storage, hash)
{
    if (check_hash(hash)) {
        init(hash);
    } else {
        blank(2);
    }
}

auto PeerReplies::Delete(const UnallocatedCString& id) -> bool
{
    return delete_item(id);
}

void PeerReplies::init(const UnallocatedCString& hash)
{
    std::shared_ptr<proto::StorageNymList> serialized;
    driver_.LoadProto(hash, serialized);

    if (!serialized) {
        std::cerr << __func__ << ": Failed to load peer reply index file."
                  << std::endl;
        abort();
    }

    init_version(2, *serialized);

    for (const auto& it : serialized->nym()) {
        item_map_.emplace(
            it.itemid(), Metadata{it.hash(), it.alias(), 0, false});
    }
}

auto PeerReplies::Load(
    const UnallocatedCString& id,
    std::shared_ptr<proto::PeerReply>& output,
    const bool checking) const -> bool
{
    UnallocatedCString notUsed;

    bool loaded = load_proto<proto::PeerReply>(id, output, notUsed, true);

    if (loaded) { return true; }

    // The provided ID might actually be a request ID instead of a reply ID.

    std::unique_lock<std::mutex> lock(write_lock_);
    UnallocatedCString realID;

    for (const auto& it : item_map_) {
        const auto& reply = it.first;
        const auto& alias = std::get<1>(it.second);

        if (id == alias) {
            realID = reply;
            break;
        }
    }

    lock.unlock();

    if (realID.empty()) { return false; }

    return load_proto<proto::PeerReply>(realID, output, notUsed, checking);
}

auto PeerReplies::save(const std::unique_lock<std::mutex>& lock) const -> bool
{
    if (!verify_write_lock(lock)) {
        std::cerr << __func__ << ": Lock failure." << std::endl;
        abort();
    }

    auto serialized = serialize();

    if (!proto::Validate(serialized, VERBOSE)) { return false; }

    return driver_.StoreProto(serialized, root_);
}

auto PeerReplies::serialize() const -> proto::StorageNymList
{
    proto::StorageNymList serialized;
    serialized.set_version(version_);

    for (const auto& item : item_map_) {
        const bool goodID = !item.first.empty();
        const bool goodHash = check_hash(std::get<0>(item.second));
        const bool good = goodID && goodHash;

        if (good) {
            serialize_index(
                version_, item.first, item.second, *serialized.add_nym());
        }
    }

    return serialized;
}

auto PeerReplies::Store(const proto::PeerReply& data) -> bool
{
    return store_proto(data, data.id(), data.cookie());
}
}  // namespace opentxs::storage
