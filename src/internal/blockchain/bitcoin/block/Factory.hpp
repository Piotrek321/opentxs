// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/endian/buffers.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>

#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/bitcoin/block/Script.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Time.hpp"

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
namespace bitcoin
{
namespace block
{
namespace internal
{
class Header;
class Input;
class Inputs;
class Output;
class Outputs;
class Script;
class Transaction;
}  // namespace internal

class Block;
class Header;
class Input;
class Inputs;
class Output;
class Outputs;
class Script;
class Transaction;
}  // namespace block

class Inventory;
struct EncodedInput;
struct EncodedOutpoint;
struct EncodedOutput;
struct EncodedTransaction;
}  // namespace bitcoin

namespace block
{
class Hash;
class Header;
class Outpoint;
}  // namespace block
}  // namespace blockchain

namespace identifier
{
class Generic;
}  // namespace identifier

namespace network
{
namespace blockchain
{
namespace bitcoin
{
class CompactSize;
}  // namespace bitcoin
}  // namespace blockchain
}  // namespace network

namespace proto
{
class BlockchainBlockHeader;
class BlockchainTransaction;
class BlockchainTransactionInput;
class BlockchainTransactionOutput;
}  // namespace proto

class Log;
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::factory
{
using UTXO = std::pair<
    blockchain::block::Outpoint,
    std::unique_ptr<blockchain::bitcoin::block::Output>>;
using Transaction_p =
    std::shared_ptr<const opentxs::blockchain::bitcoin::block::Transaction>;
using AbortFunction = std::function<bool()>;

auto BitcoinBlock(
    const api::Session& api,
    const opentxs::blockchain::block::Header& previous,
    const Transaction_p generationTransaction,
    const std::uint32_t nBits,
    const UnallocatedVector<Transaction_p>& extraTransactions,
    const std::int32_t version,
    const AbortFunction abort) noexcept
    -> std::shared_ptr<const opentxs::blockchain::bitcoin::block::Block>;
auto BitcoinBlock(
    const api::Session& api,
    const blockchain::Type chain,
    const ReadView in) noexcept
    -> std::shared_ptr<blockchain::bitcoin::block::Block>;
auto BitcoinBlockHeader(
    const api::Session& api,
    const opentxs::blockchain::block::Header& previous,
    const std::uint32_t nBits,
    const std::int32_t version,
    opentxs::blockchain::block::Hash&& merkle,
    const AbortFunction abort) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::Header>;
auto BitcoinBlockHeader(
    const api::Session& api,
    const proto::BlockchainBlockHeader& serialized) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::Header>;
auto BitcoinBlockHeader(
    const api::Session& api,
    const blockchain::Type chain,
    const ReadView bytes) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::Header>;
auto BitcoinBlockHeader(
    const api::Session& api,
    const blockchain::Type chain,
    const blockchain::block::Hash& hash,
    const blockchain::block::Hash& parent,
    const blockchain::block::Height height) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::Header>;
auto BitcoinScript(
    const blockchain::Type chain,
    const ReadView bytes,
    const blockchain::bitcoin::block::Script::Position role,
    const bool allowInvalidOpcodes = true,
    const bool mute = false) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Script>;
auto BitcoinScript(
    const blockchain::Type chain,
    blockchain::bitcoin::block::ScriptElements&& elements,
    const blockchain::bitcoin::block::Script::Position role) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Script>;
auto BitcoinTransaction(
    const api::Session& api,
    const blockchain::Type chain,
    const Time& time,
    const boost::endian::little_int32_buf_t& version,
    const boost::endian::little_uint32_buf_t lockTime,
    bool segwit,
    std::unique_ptr<blockchain::bitcoin::block::internal::Inputs> inputs,
    std::unique_ptr<blockchain::bitcoin::block::internal::Outputs>
        outputs) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Transaction>;
auto BitcoinTransaction(
    const api::Session& api,
    const blockchain::Type chain,
    const std::size_t position,
    const Time& time,
    blockchain::bitcoin::EncodedTransaction&& parsed) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Transaction>;
auto BitcoinTransaction(
    const api::Session& api,
    const proto::BlockchainTransaction& serialized) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Transaction>;
auto BitcoinTransactionInput(
    const api::Session& api,
    const blockchain::Type chain,
    const ReadView outpoint,
    const network::blockchain::bitcoin::CompactSize& cs,
    const ReadView script,
    const ReadView sequence,
    const bool isGeneration,
    UnallocatedVector<Space>&& witness) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Input>;
auto BitcoinTransactionInput(
    const api::Session& api,
    const blockchain::Type chain,
    const UTXO& spends,
    const std::optional<std::uint32_t> sequence = {}) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Input>;
auto BitcoinTransactionInput(
    const api::Session& api,
    const blockchain::Type chain,
    const proto::BlockchainTransactionInput,
    const bool isGeneration) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Input>;
auto BitcoinTransactionInputs(
    UnallocatedVector<
        std::unique_ptr<blockchain::bitcoin::block::internal::Input>>&& inputs,
    std::optional<std::size_t> size = {}) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Inputs>;
auto BitcoinTransactionOutput(
    const api::Session& api,
    const blockchain::Type chain,
    const std::uint32_t index,
    const blockchain::Amount& value,
    std::unique_ptr<const blockchain::bitcoin::block::internal::Script> script,
    const UnallocatedSet<blockchain::crypto::Key>& keys) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Output>;
auto BitcoinTransactionOutput(
    const api::Session& api,
    const blockchain::Type chain,
    const std::uint32_t index,
    const blockchain::Amount& value,
    const network::blockchain::bitcoin::CompactSize& cs,
    const ReadView script) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Output>;
auto BitcoinTransactionOutput(
    const api::Session& api,
    const blockchain::Type chain,
    const proto::BlockchainTransactionOutput& in) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Output>;
auto BitcoinTransactionOutputs(
    UnallocatedVector<std::unique_ptr<
        blockchain::bitcoin::block::internal::Output>>&& outputs,
    std::optional<std::size_t> size = {}) noexcept
    -> std::unique_ptr<blockchain::bitcoin::block::internal::Outputs>;
}  // namespace opentxs::factory
