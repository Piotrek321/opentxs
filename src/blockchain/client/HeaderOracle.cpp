// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Internal.hpp"

#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/client/HeaderOracle.hpp"
#include "opentxs/blockchain/NumericHash.hpp"
#include "opentxs/blockchain/Work.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Log.hpp"

#include "internal/api/Api.hpp"
#include "internal/blockchain/Blockchain.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <tuple>

#include "HeaderOracle.hpp"

#define OT_METHOD "opentxs::blockchain::client::implementation::HeaderOracle::"

namespace opentxs
{
blockchain::client::internal::HeaderOracle* Factory::HeaderOracle(
    const api::internal::Core& api,
    const blockchain::client::internal::Network& network,
    const blockchain::client::internal::HeaderDatabase& database,
    const blockchain::Type type)
{
    using ReturnType = blockchain::client::implementation::HeaderOracle;

    return new ReturnType(api, network, database, type);
}
}  // namespace opentxs

namespace opentxs::blockchain::client
{
const std::map<Type, block::pHash> genesis_hashes_{
    {Type::Bitcoin,
     Data::Factory(
         "0x6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000",
         Data::Mode::Hex)},
    {Type::Bitcoin_testnet3,
     Data::Factory(
         "0x43497fd7f826957108f4a30fd9cec3aeba79972084e90ead01ea330900000000",
         Data::Mode::Hex)},
    {Type::BitcoinCash,
     Data::Factory(
         "0x6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000",
         Data::Mode::Hex)},
    {Type::BitcoinCash_testnet3,
     Data::Factory(
         "0x43497fd7f826957108f4a30fd9cec3aeba79972084e90ead01ea330900000000",
         Data::Mode::Hex)},
    {Type::Ethereum_frontier,
     Data::Factory(
         "0xd4e56740f876aef8c010b86a40d5f56745a118d0906a34e69aec8c0db1cb8fa3",
         Data::Mode::Hex)},
    {Type::Ethereum_ropsten,
     Data::Factory(
         "0x41941023680923e0fe4d74a34bdac8141f2540e3ae90623718e47d66d1ca4a2d",
         Data::Mode::Hex)},
};

const block::Hash& HeaderOracle::GenesisBlockHash(const blockchain::Type type)
{
    return genesis_hashes_.at(type);
}
}  // namespace opentxs::blockchain::client

namespace opentxs::blockchain::client::implementation
{
HeaderOracle::HeaderOracle(
    const api::internal::Core& api,
    const internal::Network& network,
    const internal::HeaderDatabase& database,
    const blockchain::Type type) noexcept
    : internal::HeaderOracle()
    , api_(api)
    , network_(network)
    , database_(database)
    , chain_(type)
    , lock_()
{
}

bool HeaderOracle::AddCheckpoint(
    const block::Height position,
    const block::Hash& requiredHash) noexcept
{
    Lock lock(lock_);
    block::Position checkpoint{position, requiredHash};
    std::unique_ptr<internal::UpdateTransaction> pUpdate{
        Factory::UpdateTransaction(api_)};

    OT_ASSERT(pUpdate);

    auto& update = *pUpdate;

    if (database_.HaveCheckpoint()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Checkpoint already exists")
            .Flush();

        return false;
    }

    if (database_.HeaderExists(requiredHash)) {
        try {
            auto& header =
                update.ModifyExistingBlock(database_.LoadHeader(requiredHash));
            header.CompareToCheckpoint(checkpoint);
        } catch (...) {
            return false;
        }
    }

    const auto currentPosition = database_.CurrentBest()->Position();
    const auto [height, hash] = currentPosition;

    if (position <= height) {
        try {
            if (requiredHash != database_.BestBlock(position)) {
                update.AddSibling(currentPosition);
                auto& header =
                    update.ModifyExistingBlock(database_.LoadHeader(hash));
                blacklist_to_checkpoint(lock, checkpoint, header, update);
                reorg_to_checkpoint(lock, checkpoint, update);
                update.SetReorg(true);
            }
        } catch (...) {
            return false;
        }
    }

    update.SetCheckpoint(std::move(checkpoint));

    return database_.ApplyUpdate(std::move(pUpdate));
}

bool HeaderOracle::AddHeader(std::unique_ptr<block::Header> pHeader) noexcept
{
    if (false == bool(pHeader)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid header").Flush();

        return false;
    }

    auto& header = *pHeader;
    Lock lock(lock_);
    const auto& incomingHash = header.Hash();
    const auto& incomingParent = header.ParentHash();
    auto pCurrent = database_.CurrentBest();
    const auto& current = *pCurrent;
    const auto [currentHeight, currentHash] = current.Position();
    std::unique_ptr<internal::UpdateTransaction> pUpdate{
        Factory::UpdateTransaction(api_)};

    OT_ASSERT(pUpdate);

    auto& update = *pUpdate;

    if (database_.HeaderExists(incomingHash)) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Header already exists").Flush();

        return true;
    }

    bool addDisconnected{false};
    const auto pParent = database_.TryLoadHeader(incomingParent);

    if (pParent) {
        addDisconnected = pParent->IsDisconnected();
    } else {
        addDisconnected = true;
    }

    if (addDisconnected) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Adding disconnected header")
            .Flush();

        return insert_disconnected_block(
            lock, std::move(pHeader), std::move(pUpdate));
    }

    const auto& parent = *pParent;
    auto isCandidate = connect_to_parent(lock, parent, header);
    const bool extendsCurrentBest = (incomingParent == currentHash);
    const bool extendsSibling = database_.IsSibling(incomingParent);

    if (extendsSibling) { update.RemoveSibling(incomingParent); }

    if (isCandidate) { update.SetTip(header.Position()); }

    std::unique_ptr<const block::Header> pCandidate{header.clone()};

    if (database_.HasDisconnectedChildren(incomingHash)) {
        try {
            auto tip = scan_disconnected(
                lock, isCandidate, header, update, pCandidate);

            if (-1 != tip.first) {
                update.SetTip(std::move(tip));
                isCandidate = true;
            }
        } catch (...) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to connect children")
                .Flush();

            return false;
        }
    }

    OT_ASSERT(pCandidate);

    const auto& candidate = *pCandidate;

    if (isCandidate) {
        update.SetTipBest(evaluate_candidate(current, candidate));
    }

    if (update.TipIsBest()) {
        if (false == extendsCurrentBest) {
            update.AddSibling(current.Position());
        }

        if (false == calculate_reorg(lock, header, update)) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to calculate reorg")
                .Flush();

            return false;
        }
    } else {
        update.AddSibling({header.Height(), incomingHash});
    }

    return database_.ApplyUpdate(std::move(pHeader), std::move(pUpdate));
}

block::Position HeaderOracle::best_chain(const Lock& lock) const noexcept
{
    return database_.CurrentBest()->Position();
}

block::Position HeaderOracle::BestChain() const noexcept
{
    Lock lock(lock_);

    return best_chain(lock);
}

block::pHash HeaderOracle::BestHash(const block::Height height) const noexcept
{
    Lock lock(lock_);

    try {
        return database_.BestBlock(height);
    } catch (...) {
        return make_blank<block::pHash>::value(api_);
    }
}

void HeaderOracle::blacklist_to_checkpoint(
    const Lock& lock,
    const block::Position& checkpoint,
    block::Header& header,
    internal::UpdateTransaction& update)
{
    const auto height{header.Height()};
    const auto checkpointHeight{checkpoint.first};

    if (height == checkpointHeight) {
        header.CompareToCheckpoint(checkpoint);

        return;
    }

    auto& parent =
        update.ModifyExistingBlock(database_.LoadHeader(header.ParentHash()));
    blacklist_to_checkpoint(lock, checkpoint, parent, update);
    header.InheritState(parent);
}

bool HeaderOracle::calculate_reorg(
    const Lock& lock,
    const block::Header& header,
    internal::UpdateTransaction& update) noexcept
{
    OT_ASSERT(0 < header.Height());

    update.AddToBestChain(update.Tip());
    auto height{header.Height() - 1};
    block::pHash hash{header.ParentHash()};

    while (true) {
        try {
            if (database_.BestBlock(height) == hash) {
                update.SetReorgParent({height, hash});
                break;
            } else {
                if (0 == height) {
                    LogOutput(OT_METHOD)(__FUNCTION__)(
                        ": Reorg of genesis block is not allowed")
                        .Flush();

                    return false;
                } else {
                    update.SetReorg(true);
                    update.AddToBestChain(height, hash);
                }
            }
        } catch (...) {
            update.AddToBestChain(height, hash);
        }

        try {
            hash = update.Header(hash).ParentHash();
        } catch (...) {
            try {
                hash = database_.LoadHeader(hash)->ParentHash();
            } catch (...) {
                LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to load header")
                    .Flush();

                return false;
            }
        }

        --height;
    }

    return true;
}

std::pair<block::Position, block::Position> HeaderOracle::CommonParent(
    const block::Position& position) const noexcept
{
    Lock lock(lock_);
    const auto& database = database_;
    std::pair<block::Position, block::Position> output{
        {0, GenesisBlockHash(chain_)}, best_chain(lock)};
    auto& [parent, best] = output;
    auto test{position};
    auto pHeader = database.TryLoadHeader(test.second);

    if (false == bool(pHeader)) { return output; }

    while (0 < test.first) {
        if (is_in_best_chain(lock, test.second)) {
            parent = test;

            return output;
        }

        pHeader = database.TryLoadHeader(pHeader->ParentHash());

        if (pHeader) {
            test = pHeader->Position();
        } else {
            return output;
        }
    }

    return output;
}

void HeaderOracle::connect_children(
    const Lock& lock,
    block::Header& parentHeader,
    std::vector<std::unique_ptr<block::Header>>& reconnectedTips,
    internal::UpdateTransaction& update)
{
    const auto disconnected = database_.DisconnectedHashes();
    const auto [first, last] = disconnected.equal_range(parentHeader.Hash());
    const auto children = std::distance(first, last);

    if (0 == children) {
        reconnectedTips.emplace_back(parentHeader.clone());
    } else {
        std::for_each(first, last, [&](const auto& in) -> void {
            const auto& [parentHash, childHash] = in;
            update.ConnectBlock({parentHash, childHash});
            auto& childHeader =
                update.ModifyExistingBlock(database_.LoadHeader(childHash));
            connect_to_parent(lock, parentHeader, childHeader);
            connect_children(lock, childHeader, reconnectedTips, update);
        });
    }
}

bool HeaderOracle::connect_to_parent(
    const Lock& lock,
    const block::Header& parent,
    block::Header& child) noexcept
{
    child.InheritWork(parent.Work());
    child.InheritState(parent);
    child.InheritHeight(parent);
    child.CompareToCheckpoint(database_.CurrentCheckpoint());

    if (child.IsBlacklisted()) { return false; }

    return true;
}

bool HeaderOracle::DeleteCheckpoint() noexcept
{
    Lock lock(lock_);

    if (false == database_.HaveCheckpoint()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": No checkpoint").Flush();

        return false;
    }

    std::unique_ptr<internal::UpdateTransaction> pUpdate{
        Factory::UpdateTransaction(api_)};

    OT_ASSERT(pUpdate);

    auto& update = *pUpdate;
    update.ClearCheckpoint();
    const auto checkpoint = database_.CurrentCheckpoint();

    try {
        if (database_.HeaderExists(checkpoint.second)) {
            auto& checkpointBlock = update.ModifyExistingBlock(
                database_.LoadHeader(checkpoint.second));
            checkpointBlock.RemoveCheckpointState();
            std::vector<block::Position> candidateTips{};

            for (const auto& hash : database_.SiblingHashes()) {
                auto pHeader = database_.LoadHeader(hash);
                auto& header = *pHeader;
                const auto blacklisted = header.IsBlacklisted();

                if (blacklisted) {
                    candidateTips.emplace_back(header.Position());
                    auto& modified =
                        update.ModifyExistingBlock(std::move(pHeader));
                    reverse_blacklist(lock, modified, update);
                }
            }

            auto pPrevious = database_.CurrentBest();
            const auto& previous = *pPrevious;
            update.SetTip(previous.Position());
            const auto current = update.Tip();
            auto& tip = update.Tip();

            for (const auto& position : candidateTips) {
                auto pHeader = database_.LoadHeader(position.second);
                auto& header = *pHeader;

                if (evaluate_candidate(previous, header)) {
                    pPrevious = std::move(pHeader);
                    update.SetTip(position);
                }
            }

            if (current != tip) {
                calculate_reorg(
                    lock, *database_.LoadHeader(tip.second), update);
                update.SetReorg(true);
                update.AddSibling(current);
            }
        }
    } catch (...) {
        return false;
    }

    return database_.ApplyUpdate(std::move(pUpdate));
}

bool HeaderOracle::evaluate_candidate(
    const block::Header& current,
    const block::Header& candidate) noexcept
{
    return candidate.Work() > current.Work();
}

block::Position HeaderOracle::GetCheckpoint() const noexcept
{
    Lock lock(lock_);

    return database_.CurrentCheckpoint();
}

bool HeaderOracle::insert_disconnected_block(
    const Lock& lock,
    std::unique_ptr<block::Header> header,
    std::unique_ptr<internal::UpdateTransaction> pUpdate) noexcept
{
    OT_ASSERT(header);
    OT_ASSERT(pUpdate);

    auto& update = *pUpdate;
    header->SetDisconnectedState();
    update.DisconnectBlock(*header);

    return database_.ApplyUpdate(std::move(header), std::move(pUpdate));
}

bool HeaderOracle::IsInBestChain(const block::Hash& hash) const noexcept
{
    Lock lock(lock_);

    return is_in_best_chain(lock, hash);
}

bool HeaderOracle::is_in_best_chain(const Lock& lock, const block::Hash& hash)
    const noexcept
{
    const auto pHeader = database_.TryLoadHeader(hash);

    if (false == bool(pHeader)) { return false; }

    const auto& header = *pHeader;

    try {
        return hash == database_.BestBlock(header.Height());

    } catch (...) {

        return false;
    }
}

std::unique_ptr<block::Header> HeaderOracle::LoadHeader(
    const block::Hash& hash) const noexcept
{
    return database_.TryLoadHeader(hash);
}

bool HeaderOracle::reorg_to_checkpoint(
    const Lock& lock,
    const block::Position& checkpoint,
    internal::UpdateTransaction& update)
{
    const auto& tip = update.Tip();
    bool checkpointIsBest{false};
    std::vector<block::Position> candidateTips{};

    for (const auto& hash : database_.SiblingHashes()) {
        auto pHeader = database_.LoadHeader(hash);
        const auto& header = *pHeader;

        if (header.Height() < checkpoint.first) { continue; }

        if (header.Height() == checkpoint.first) {
            auto& modified = update.ModifyExistingBlock(std::move(pHeader));
            modified.CompareToCheckpoint(checkpoint);
            checkpointIsBest = true;
            update.RemoveSibling(hash);

            continue;
        }

        auto containsCheckpoint = scan_for_checkpoint(lock, checkpoint, header);

        if (containsCheckpoint) {
            candidateTips.emplace_back(header.Position());
        } else {
            auto& modified = update.ModifyExistingBlock(std::move(pHeader));
            blacklist_to_checkpoint(lock, checkpoint, modified, update);
        }
    }

    if (checkpointIsBest) {
        update.SetTip(checkpoint);
    } else {
        const block::Height beforeCheckpoint{checkpoint.first - 1};
        update.SetTip(
            {beforeCheckpoint, database_.BestBlock(beforeCheckpoint)});

        for (const auto& position : candidateTips) {
            const auto currentHeader = database_.LoadHeader(tip.second);
            const auto candidateHeader = database_.LoadHeader(position.second);

            if (evaluate_candidate(*currentHeader, *candidateHeader)) {
                update.SetTip(position);
            }
        }
    }

    return calculate_reorg(lock, *database_.LoadHeader(tip.second), update);
}

void HeaderOracle::reverse_blacklist(
    const Lock& lock,
    block::Header& header,
    internal::UpdateTransaction& update)
{
    if (0 == header.Height()) { return; }

    const bool recurse = header.IsBlacklisted();
    header.RemoveBlacklistState();

    if (recurse) {
        auto& parent = update.ModifyExistingBlock(
            database_.LoadHeader(header.ParentHash()));
        reverse_blacklist(lock, parent, update);
    }
}

block::Position HeaderOracle::scan_disconnected(
    const Lock& lock,
    const bool isCandidate,
    block::Header& parent,
    internal::UpdateTransaction& update,
    std::unique_ptr<const block::Header>& candidate)
{
    std::vector<std::unique_ptr<block::Header>> reconnectedTips{};
    connect_children(lock, parent, reconnectedTips, update);
    auto potentialTip =
        (isCandidate) ? parent.clone() : std::unique_ptr<block::Header>();

    for (auto& pHeader : reconnectedTips) {
        const auto& child = *pHeader;
        const auto position = child.Position();
        const auto status = child.EffectiveState();

        update.AddSibling(position);
        const auto candidate =
            block::Header::Status::CheckpointBanned != status;

        if (candidate) {
            if (block::Header::Status::Checkpoint == status) {
                potentialTip = std::move(pHeader);
            } else if (false == bool(potentialTip)) {
                potentialTip = std::move(pHeader);
            } else if (evaluate_candidate(*potentialTip, child)) {
                potentialTip = std::move(pHeader);
            }
        }
    }

    if (potentialTip) {
        const auto output = potentialTip->Position();
        candidate = std::move(potentialTip);

        return output;
    } else {

        return make_blank<block::Position>::value(api_);
    }
}

bool HeaderOracle::scan_for_checkpoint(
    const Lock& lock,
    const block::Position& checkpoint,
    const block::Header& header)
{
    const auto height = header.Height();
    const auto& hash = header.Hash();
    const auto& parentHash = header.ParentHash();

    OT_ASSERT(height >= checkpoint.first);

    if (height == checkpoint.first) { return hash == checkpoint.second; }

    return scan_for_checkpoint(
        lock, checkpoint, *database_.LoadHeader(parentHash));
}

std::set<block::pHash> HeaderOracle::Siblings() const noexcept
{
    Lock lock(lock_);

    return database_.SiblingHashes();
}
}  // namespace opentxs::blockchain::client::implementation
