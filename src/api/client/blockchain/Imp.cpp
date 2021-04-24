// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                   // IWYU pragma: associated
#include "1_Internal.hpp"                 // IWYU pragma: associated
#include "api/client/blockchain/Imp.hpp"  // IWYU pragma: associated

#include <bech32.h>
#include <boost/container/flat_map.hpp>
#include <segwit_addr.h>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "internal/api/Api.hpp"
#include "internal/api/client/Client.hpp"
#include "internal/api/client/blockchain/Blockchain.hpp"
#include "internal/blockchain/Params.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/blockchain/AddressStyle.hpp"
#include "opentxs/api/client/blockchain/HD.hpp"
#include "opentxs/api/client/blockchain/PaymentCode.hpp"
#include "opentxs/api/crypto/Crypto.hpp"
#include "opentxs/api/crypto/Encode.hpp"
#include "opentxs/api/crypto/Hash.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/blockchain/block/bitcoin/Transaction.hpp"  // IWYU pragma: keep
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/crypto/Bip32.hpp"
#include "opentxs/crypto/Bip32Child.hpp"
#include "opentxs/crypto/Bip43Purpose.hpp"
#include "opentxs/crypto/Bip44Type.hpp"
#include "opentxs/crypto/HashType.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/protobuf/BlockchainP2PHello.pb.h"
#include "opentxs/protobuf/HDPath.pb.h"
#include "util/Container.hpp"
#include "util/HDIndex.hpp"

#define PATH_VERSION 1
#define COMPRESSED_PUBKEY_SIZE 33

#define OT_METHOD "opentxs::api::client::implementation::Blockchain::Imp::"

namespace opentxs::api::client
{
enum class Prefix : std::uint8_t {
    Unknown = 0,
    BitcoinP2PKH,
    BitcoinP2SH,
    BitcoinTestnetP2PKH,
    BitcoinTestnetP2SH,
    LitecoinP2PKH,
    LitecoinP2SH,
    LitecoinTestnetP2SH,
    PKTP2PKH,
    PKTP2SH,
};

using Style = Blockchain::Style;
using Chain = Blockchain::Chain;
using AddressMap = std::map<Prefix, std::string>;
using AddressReverseMap = std::map<std::string, Prefix>;
using StylePair = std::pair<Style, Chain>;
// Style, preferred prefix, additional prefixes
using StyleMap = std::map<StylePair, std::pair<Prefix, std::set<Prefix>>>;
using StyleReverseMap = std::map<Prefix, std::set<StylePair>>;
using HrpMap = std::map<Chain, std::string>;
using HrpReverseMap = std::map<std::string, Chain>;

auto reverse(const StyleMap& in) noexcept -> StyleReverseMap;
auto reverse(const StyleMap& in) noexcept -> StyleReverseMap
{
    auto output = StyleReverseMap{};
    std::for_each(std::begin(in), std::end(in), [&](const auto& data) {
        const auto& [metadata, prefixData] = data;
        const auto& [preferred, additional] = prefixData;
        output[preferred].emplace(metadata);

        for (const auto& prefix : additional) {
            output[prefix].emplace(metadata);
        }
    });

    return output;
}

const AddressReverseMap address_prefix_reverse_map_{
    {"00", Prefix::BitcoinP2PKH},
    {"05", Prefix::BitcoinP2SH},
    {"30", Prefix::LitecoinP2PKH},
    {"32", Prefix::LitecoinP2SH},
    {"3a", Prefix::LitecoinTestnetP2SH},
    {"38", Prefix::PKTP2SH},
    {"6f", Prefix::BitcoinTestnetP2PKH},
    {"c4", Prefix::BitcoinTestnetP2SH},
    {"75", Prefix::PKTP2PKH},
};
const AddressMap address_prefix_map_{reverse_map(address_prefix_reverse_map_)};
const StyleMap address_style_map_{
    {{Style::P2PKH, Chain::UnitTest}, {Prefix::BitcoinTestnetP2PKH, {}}},
    {{Style::P2PKH, Chain::BitcoinCash_testnet3},
     {Prefix::BitcoinTestnetP2PKH, {}}},
    {{Style::P2PKH, Chain::BitcoinCash}, {Prefix::BitcoinP2PKH, {}}},
    {{Style::P2PKH, Chain::Bitcoin_testnet3},
     {Prefix::BitcoinTestnetP2PKH, {}}},
    {{Style::P2PKH, Chain::Bitcoin}, {Prefix::BitcoinP2PKH, {}}},
    {{Style::P2PKH, Chain::Litecoin_testnet4},
     {Prefix::BitcoinTestnetP2PKH, {}}},
    {{Style::P2PKH, Chain::Litecoin}, {Prefix::LitecoinP2PKH, {}}},
    {{Style::P2PKH, Chain::PKT_testnet}, {Prefix::BitcoinTestnetP2PKH, {}}},
    {{Style::P2PKH, Chain::PKT}, {Prefix::PKTP2PKH, {}}},
    {{Style::P2SH, Chain::UnitTest}, {Prefix::BitcoinTestnetP2SH, {}}},
    {{Style::P2SH, Chain::BitcoinCash_testnet3},
     {Prefix::BitcoinTestnetP2SH, {}}},
    {{Style::P2SH, Chain::BitcoinCash}, {Prefix::BitcoinP2SH, {}}},
    {{Style::P2SH, Chain::Bitcoin_testnet3}, {Prefix::BitcoinTestnetP2SH, {}}},
    {{Style::P2SH, Chain::Bitcoin}, {Prefix::BitcoinP2SH, {}}},
    {{Style::P2SH, Chain::Litecoin_testnet4},
     {Prefix::LitecoinTestnetP2SH, {Prefix::BitcoinTestnetP2SH}}},
    {{Style::P2SH, Chain::Litecoin},
     {Prefix::LitecoinP2SH, {Prefix::BitcoinP2SH}}},
    {{Style::P2SH, Chain::PKT_testnet}, {Prefix::BitcoinTestnetP2SH, {}}},
    {{Style::P2SH, Chain::PKT}, {Prefix::PKTP2SH, {}}},
};
const StyleReverseMap address_style_reverse_map_{reverse(address_style_map_)};
const HrpMap hrp_map_{
    {Chain::Bitcoin, "bc"},
    {Chain::Bitcoin_testnet3, "tb"},
    {Chain::Litecoin, "ltc"},
    {Chain::Litecoin_testnet4, "tltc"},
    {Chain::PKT, "pkt"},
    {Chain::PKT_testnet, "tpk"},
    {Chain::UnitTest, "bcrt"},
};
const HrpReverseMap hrp_reverse_map_{reverse_map(hrp_map_)};
}  // namespace opentxs::api::client

namespace opentxs::api::client::implementation
{
Blockchain::Imp::Imp(
    const api::internal::Core& api,
    const api::client::Contacts& contacts,
    api::client::internal::Blockchain& parent) noexcept
    : api_(api)
    , contacts_(contacts)
    , blank_(api_.Factory().Data(), Style::Unknown, {}, false)
    , lock_()
    , nym_lock_()
    , accounts_(api_)
    , balance_lists_(api_, parent)
{
}

auto Blockchain::Imp::ActivityDescription(
    const identifier::Nym&,
    const Identifier&,
    const std::string&) const noexcept -> std::string
{
    return {};
}

auto Blockchain::Imp::ActivityDescription(
    const identifier::Nym&,
    const Chain,
    const Tx&) const noexcept -> std::string
{
    return {};
}

auto Blockchain::Imp::address_prefix(const Style style, const Chain chain) const
    noexcept(false) -> OTData
{
    return api_.Factory().Data(
        address_prefix_map_.at(address_style_map_.at({style, chain}).first),
        StringStyle::Hex);
}

auto Blockchain::Imp::AssignContact(
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const blockchain::Subchain subchain,
    const Bip32Index index,
    const Identifier& contactID) const noexcept -> bool
{
    if (false == validate_nym(nymID)) { return false; }

    auto lock = Lock{nym_mutex(nymID)};

    const auto chain = Translate(
        api_.Storage().BlockchainAccountType(nymID.str(), accountID.str()));

    OT_ASSERT(Chain::Unknown != chain);

    try {
        auto& node = balance_lists_.Get(chain).Nym(nymID).Node(accountID);

        try {
            const auto& element = node.BalanceElement(subchain, index);
            const auto existing = element.Contact();

            if (contactID == existing) { return true; }

            return node.SetContact(subchain, index, contactID);
        } catch (...) {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Failed to load balance element")
                .Flush();

            return false;
        }
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to load account").Flush();

        return false;
    }
}

auto Blockchain::Imp::AssignLabel(
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const blockchain::Subchain subchain,
    const Bip32Index index,
    const std::string& label) const noexcept -> bool
{
    if (false == validate_nym(nymID)) { return false; }

    auto lock = Lock{nym_mutex(nymID)};

    const auto chain = Translate(
        api_.Storage().BlockchainAccountType(nymID.str(), accountID.str()));

    OT_ASSERT(Chain::Unknown != chain);

    try {
        auto& node = balance_lists_.Get(chain).Nym(nymID).Node(accountID);

        try {
            const auto& element = node.BalanceElement(subchain, index);

            if (label == element.Label()) { return true; }

            return node.SetLabel(subchain, index, label);
        } catch (...) {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Failed to load balance element")
                .Flush();

            return false;
        }
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to load account").Flush();

        return false;
    }
}

auto Blockchain::Imp::BalanceTree(
    const identifier::Nym& nymID,
    const Chain chain) const noexcept(false)
    -> const blockchain::internal::BalanceTree&
{
    if (false == validate_nym(nymID)) {
        throw std::runtime_error("Invalid nym");
    }

    if (Chain::Unknown == chain) { throw std::runtime_error("Invalid chain"); }

    auto& balanceList = balance_lists_.Get(chain);

    return balanceList.Nym(nymID);
}

auto Blockchain::Imp::bip44_type(
    const contact::ContactItemType type) const noexcept -> Bip44Type
{
    switch (type) {
        case contact::ContactItemType::BTC: {

            return Bip44Type::BITCOIN;
        }
        case contact::ContactItemType::LTC: {

            return Bip44Type::LITECOIN;
        }
        case contact::ContactItemType::DOGE: {

            return Bip44Type::DOGECOIN;
        }
        case contact::ContactItemType::DASH: {

            return Bip44Type::DASH;
        }
        case contact::ContactItemType::BCH: {

            return Bip44Type::BITCOINCASH;
        }
        case contact::ContactItemType::PKT: {

            return Bip44Type::PKT;
        }
        case contact::ContactItemType::TNBCH:
        case contact::ContactItemType::TNBTC:
        case contact::ContactItemType::TNXRP:
        case contact::ContactItemType::TNLTX:
        case contact::ContactItemType::TNXEM:
        case contact::ContactItemType::TNDASH:
        case contact::ContactItemType::TNMAID:
        case contact::ContactItemType::TNLSK:
        case contact::ContactItemType::TNDOGE:
        case contact::ContactItemType::TNXMR:
        case contact::ContactItemType::TNWAVES:
        case contact::ContactItemType::TNNXT:
        case contact::ContactItemType::TNSC:
        case contact::ContactItemType::TNSTEEM:
        case contact::ContactItemType::TNPKT:
        case contact::ContactItemType::Regtest: {
            return Bip44Type::TESTNET;
        }
        default: {
            OT_FAIL;
        }
    }
}

auto Blockchain::Imp::BlockchainDB() const noexcept
    -> const blockchain::database::implementation::Database&
{
    OT_FAIL;
}

auto Blockchain::Imp::CalculateAddress(
    const Chain chain,
    const Style format,
    const Data& pubkey) const noexcept -> std::string
{
    auto data = api_.Factory().Data();

    switch (format) {
        case Style::P2PKH: {
            try {
                data = PubkeyHash(chain, pubkey);
            } catch (...) {
                LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid public key.")
                    .Flush();

                return {};
            }
        } break;
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported address style (")(
                static_cast<std::uint16_t>(format))(")")
                .Flush();

            return {};
        }
    }

    return EncodeAddress(format, chain, data);
}

auto Blockchain::Imp::Confirm(
    const blockchain::Key key,
    const opentxs::blockchain::block::Txid& tx) const noexcept -> bool
{
    try {
        const auto [id, subchain, index] = key;
        const auto accountID = api_.Factory().Identifier(id);

        return get_node(accountID).Confirm(subchain, index, tx);
    } catch (...) {

        return false;
    }
}

auto Blockchain::Imp::DecodeAddress(const std::string& encoded) const noexcept
    -> DecodedAddress
{
    static constexpr auto check =
        [](DecodedAddress& output) -> DecodedAddress& {
        auto& [data, style, chains, supported] = output;
        supported = false;

        if (0 == data->size()) { return output; }
        if (Style::Unknown == style) { return output; }
        if (0 == chains.size()) { return output; }

        const auto& params = opentxs::blockchain::params::Data::Chains();

        for (const auto& chain : chains) {
            try {
                if (false == params.at(chain).scripts_.at(style)) {

                    return output;
                }
            } catch (...) {

                return output;
            }
        }

        supported = true;

        return output;
    };
    auto output = decode_bech23(encoded);

    if (output.has_value()) { return check(output.value()); }

    output = decode_legacy(encoded);

    if (output.has_value()) { return check(output.value()); }

    return blank_;
}

auto Blockchain::Imp::decode_bech23(const std::string& encoded) const noexcept
    -> std::optional<DecodedAddress>
{
    auto output{blank_};
    auto& [data, style, chains, supported] = output;

    try {
        const auto result = bech32::decode(encoded);
        using Encoding = bech32::Encoding;

        switch (result.encoding) {
            case Encoding::BECH32:
            case Encoding::BECH32M: {
            } break;
            case Encoding::INVALID:
            default: {
                throw std::runtime_error("not bech32");
            }
        }

        const auto [version, bytes] = segwit_addr::decode(result.hrp, encoded);

        try {
            switch (version) {
                case 0: {
                    switch (bytes.size()) {
                        case 20: {
                            style = Style::P2WPKH;
                        } break;
                        case 32: {
                            style = Style::P2WSH;
                        } break;
                        default: {
                            throw std::runtime_error{
                                "unknown version 0 program"};
                        }
                    }
                } break;
                case 1: {
                    switch (bytes.size()) {
                        case 32: {
                            style = Style::P2TR;
                        } break;
                        default: {
                            throw std::runtime_error{
                                "unknown version 1 program"};
                        }
                    }
                } break;
                case -1:
                default: {
                    throw std::runtime_error{"Unsupported version"};
                }
            }

            copy(reader(bytes), data->WriteInto());
            chains.emplace(hrp_reverse_map_.at(result.hrp));

            return std::move(output);
        } catch (const std::exception& e) {
            LogTrace(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

            return blank_;
        }
    } catch (const std::exception& e) {
        LogTrace(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return std::nullopt;
    }
}

auto Blockchain::Imp::decode_legacy(const std::string& encoded) const noexcept
    -> std::optional<DecodedAddress>
{
    auto output{blank_};
    auto& [data, style, chains, supported] = output;

    try {
        const auto bytes = api_.Factory().Data(
            api_.Crypto().Encode().IdentifierDecode(encoded), StringStyle::Raw);
        auto type = api_.Factory().Data();

        if (0 == bytes->size()) { throw std::runtime_error("not base58"); }

        try {
            switch (bytes->size()) {
                case 21: {
                    bytes->Extract(1, type, 0);
                    auto prefix{Prefix::Unknown};

                    try {
                        prefix = address_prefix_reverse_map_.at(type->asHex());
                    } catch (...) {
                        throw std::runtime_error(
                            "unable to decode version byte");
                    }

                    const auto& map = address_style_reverse_map_.at(prefix);

                    for (const auto& [decodeStyle, decodeChain] : map) {
                        style = decodeStyle;
                        chains.emplace(decodeChain);
                    }

                    bytes->Extract(20, data, 1);
                } break;
                default: {
                    throw std::runtime_error("unknown address format");
                }
            }

            return std::move(output);
        } catch (const std::exception& e) {
            LogTrace(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

            return blank_;
        }
    } catch (const std::exception& e) {
        LogTrace(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return std::nullopt;
    }
}

auto Blockchain::Imp::Disable(const Chain) const noexcept -> bool
{
    return false;
}

auto Blockchain::Imp::Enable(const Chain, const std::string& seednode)
    const noexcept -> bool
{
    return false;
}

auto Blockchain::Imp::EnabledChains() const noexcept -> std::set<Chain>
{
    return {};
}

auto Blockchain::Imp::EncodeAddress(
    const Style style,
    const Chain chain,
    const Data& data) const noexcept -> std::string
{
    switch (style) {
        case Style::P2PKH: {

            return p2pkh(chain, data);
        }
        case Style::P2SH: {

            return p2sh(chain, data);
        }
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported address style (")(
                static_cast<std::uint16_t>(style))(")")
                .Flush();

            return {};
        }
    }
}

auto Blockchain::Imp::GetChain(const Chain type) const noexcept(false)
    -> const opentxs::blockchain::Network&
{
    throw std::out_of_range{"No blockchain support"};
}

auto Blockchain::Imp::GetKey(const blockchain::Key& id) const noexcept(false)
    -> const blockchain::BalanceNode::Element&
{
    const auto [str, subchain, index] = id;
    const auto account = api_.Factory().Identifier(str);

    switch (accounts_.Type(account)) {
        case AccountType::HD: {
            const auto& hd = HDSubaccount(accounts_.Owner(account), account);

            return hd.BalanceElement(subchain, index);
        }
        case AccountType::PaymentCode: {
            const auto& pc =
                PaymentCodeSubaccount(accounts_.Owner(account), account);

            return pc.BalanceElement(subchain, index);
        }
        case AccountType::Imported:
        case AccountType::Error:
        default: {
        }
    }

    throw std::out_of_range("key not found");
}

auto Blockchain::Imp::get_node(const Identifier& accountID) const
    noexcept(false) -> blockchain::internal::BalanceNode&
{
    const auto& nymID = accounts_.Owner(accountID);

    switch (accounts_.Type(accountID)) {
        case AccountType::HD: {
            const auto type = api_.Storage().BlockchainAccountType(
                nymID.str(), accountID.str());

            if (contact::ContactItemType::Error == type) {
                throw std::out_of_range("Account does not exist");
            }

            auto& balanceList = balance_lists_.Get(Translate(type));
            auto& nym = const_cast<blockchain::internal::BalanceTree&>(
                balanceList.Nym(nymID));

            return nym.HDChain(accountID);
        }
        case AccountType::PaymentCode: {
            const auto type = api_.Storage().Bip47Chain(nymID, accountID);

            if (contact::ContactItemType::Error == type) {
                throw std::out_of_range("Account does not exist");
            }

            auto& balanceList = balance_lists_.Get(Translate(type));
            auto& nym = balanceList.Nym(nymID);

            return nym.PaymentCode(accountID);
        }
        case AccountType::Imported:
        case AccountType::Error:
        default: {
        }
    }

    throw std::out_of_range("key not found");
}

auto Blockchain::Imp::HDSubaccount(
    const identifier::Nym& nymID,
    const Identifier& accountID) const noexcept(false) -> const blockchain::HD&
{
    const auto type =
        api_.Storage().BlockchainAccountType(nymID.str(), accountID.str());

    if (contact::ContactItemType::Error == type) {
        throw std::out_of_range("Account does not exist");
    }

    auto& balanceList = balance_lists_.Get(Translate(type));
    auto& nym = balanceList.Nym(nymID);

    return nym.HDChain(accountID);
}

auto Blockchain::Imp::Hello() const noexcept -> proto::BlockchainP2PHello
{
    return {};
}

auto Blockchain::Imp::IndexItem(const ReadView bytes) const noexcept
    -> PatternID
{
    return {};
}

auto Blockchain::Imp::Init() noexcept -> void { accounts_.Populate(); }

auto Blockchain::Imp::init_path(
    const std::string& root,
    const contact::ContactItemType chain,
    const Bip32Index account,
    const BlockchainAccountType standard,
    proto::HDPath& path) const noexcept -> void
{
    path.set_version(PATH_VERSION);
    path.set_root(root);

    switch (standard) {
        case BlockchainAccountType::BIP32: {
            path.add_child(HDIndex{account, Bip32Child::HARDENED});
        } break;
        case BlockchainAccountType::BIP44: {
            path.add_child(
                HDIndex{Bip43Purpose::HDWALLET, Bip32Child::HARDENED});
            path.add_child(HDIndex{bip44_type(chain), Bip32Child::HARDENED});
            path.add_child(account);
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto Blockchain::Imp::IsEnabled(const opentxs::blockchain::Type) const noexcept
    -> bool
{
    return false;
}

auto Blockchain::Imp::KeyEndpoint() const noexcept -> const std::string&
{
    static const auto blank = std::string{};

    return blank;
}

auto Blockchain::Imp::KeyGenerated(const Chain) const noexcept -> void {}

auto Blockchain::Imp::LoadTransactionBitcoin(const TxidHex&) const noexcept
    -> std::unique_ptr<const Tx>
{
    return {};
}

auto Blockchain::Imp::LoadTransactionBitcoin(const Txid&) const noexcept
    -> std::unique_ptr<const Tx>
{
    return {};
}

auto Blockchain::Imp::LookupContacts(const Data&) const noexcept -> ContactList
{
    return {};
}

auto Blockchain::Imp::NewHDSubaccount(
    const identifier::Nym& nymID,
    const BlockchainAccountType standard,
    const Chain chain,
    const PasswordPrompt& reason) const noexcept -> OTIdentifier
{
    if (false == validate_nym(nymID)) { return Identifier::Factory(); }

    if (Chain::Unknown == chain) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid chain").Flush();

        return Identifier::Factory();
    }

    auto nym = api_.Wallet().Nym(nymID);

    if (false == bool(nym)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Nym does not exist.").Flush();

        return Identifier::Factory();
    }

    proto::HDPath nymPath{};

    if (false == nym->Path(nymPath)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": No nym path.").Flush();

        return Identifier::Factory();
    }

    if (0 == nymPath.root().size()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Missing root.").Flush();

        return Identifier::Factory();
    }

    if (2 > nymPath.child().size()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid path.").Flush();

        return Identifier::Factory();
    }

    proto::HDPath accountPath{};
    init_path(
        nymPath.root(),
        Translate(chain),
        HDIndex{nymPath.child(1), Bip32Child::HARDENED},
        standard,
        accountPath);

    try {
        auto accountID = Identifier::Factory();
        auto& tree = balance_lists_.Get(chain).Nym(nymID);
        tree.AddHDNode(accountPath, reason, accountID);
        accounts_.New(AccountType::HD, chain, accountID, nymID);

        /* FIXME
    #if OT_BLOCKCHAIN
            {
                auto work =
                    api_.ZeroMQ().TaggedMessage(WorkType::BlockchainAccountCreated);
                work->AddFrame(chain);
                work->AddFrame(nymID);
                work->AddFrame(AccountType::HD);
                work->AddFrame(accountID);
                new_blockchain_accounts_->Send(work);
            }

            balances_.RefreshBalance(nymID, chain);
    #endif  // OT_BLOCKCHAIN
        FIXME */

        return accountID;
    } catch (...) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Failed to create account")
            .Flush();

        return Identifier::Factory();
    }
}

auto Blockchain::Imp::NewPaymentCodeSubaccount(
    const identifier::Nym& nymID,
    const opentxs::PaymentCode& local,
    const opentxs::PaymentCode& remote,
    const proto::HDPath path,
    const Chain chain,
    const PasswordPrompt& reason) const noexcept -> OTIdentifier
{
    auto lock = Lock{nym_mutex(nymID)};

    return new_payment_code(lock, nymID, local, remote, path, chain, reason);
}

auto Blockchain::Imp::new_payment_code(
    const Lock&,
    const identifier::Nym& nymID,
    const opentxs::PaymentCode& local,
    const opentxs::PaymentCode& remote,
    const proto::HDPath path,
    const Chain chain,
    const PasswordPrompt& reason) const noexcept -> OTIdentifier
{
    static const auto blank = api_.Factory().Identifier();

    if (false == validate_nym(nymID)) { return blank; }

    if (Chain::Unknown == chain) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid chain").Flush();

        return blank;
    }

    auto nym = api_.Wallet().Nym(nymID);

    if (false == bool(nym)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Nym does not exist.").Flush();

        return blank;
    }

    if (0 == path.root().size()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Missing root.").Flush();

        return blank;
    }

    if (3 > path.child().size()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid path: ")(
            opentxs::crypto::Print(path))
            .Flush();

        return blank;
    }

    try {
        auto accountID = blank;
        auto& tree = balance_lists_.Get(chain).Nym(nymID);
        tree.AddUpdatePaymentCode(local, remote, path, reason, accountID);
        accounts_.New(AccountType::PaymentCode, chain, accountID, nymID);

        /* FIXME
#if OT_BLOCKCHAIN
        {
            auto work =
                api_.ZeroMQ().TaggedMessage(WorkType::BlockchainAccountCreated);
            work->AddFrame(chain);
            work->AddFrame(nymID);
            work->AddFrame(AccountType::PaymentCode);
            work->AddFrame(accountID);
            new_blockchain_accounts_->Send(work);
        }

        balances_.RefreshBalance(nymID, chain);
#endif  // OT_BLOCKCHAIN
        FIXME */

        return accountID;
    } catch (...) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Failed to create account")
            .Flush();

        return blank;
    }
}

auto Blockchain::Imp::nym_mutex(const identifier::Nym& nym) const noexcept
    -> std::mutex&
{
    auto lock = Lock{lock_};

    return nym_lock_[nym];
}

auto Blockchain::Imp::Owner(const blockchain::Key& key) const noexcept
    -> const identifier::Nym&
{
    const auto& [account, subchain, index] = key;
    static const auto blank = api_.Factory().NymID();

    if (blockchain::Subchain::Outgoing == subchain) { return blank; }

    return Owner(api_.Factory().Identifier(account));
}

auto Blockchain::Imp::p2pkh(const Chain chain, const Data& pubkeyHash)
    const noexcept -> std::string
{
    try {
        auto preimage = address_prefix(Style::P2PKH, chain);

        OT_ASSERT(1 == preimage->size());

        preimage += pubkeyHash;

        OT_ASSERT(21 == preimage->size());

        return api_.Crypto().Encode().IdentifierEncode(preimage);
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported chain (")(
            static_cast<std::uint32_t>(chain))(")")
            .Flush();

        return "";
    }
}

auto Blockchain::Imp::p2sh(const Chain chain, const Data& pubkeyHash)
    const noexcept -> std::string
{
    try {
        auto preimage = address_prefix(Style::P2SH, chain);

        OT_ASSERT(1 == preimage->size());

        preimage += pubkeyHash;

        OT_ASSERT(21 == preimage->size());

        return api_.Crypto().Encode().IdentifierEncode(preimage);
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported chain (")(
            static_cast<std::uint32_t>(chain))(")")
            .Flush();

        return "";
    }
}

auto Blockchain::Imp::PaymentCodeSubaccount(
    const identifier::Nym& nymID,
    const Identifier& accountID) const noexcept(false)
    -> const blockchain::PaymentCode&
{
    const auto type = api_.Storage().Bip47Chain(nymID, accountID);

    if (contact::ContactItemType::Error == type) {
        throw std::out_of_range("Account does not exist");
    }

    auto& balanceList = balance_lists_.Get(Translate(type));
    auto& nym = balanceList.Nym(nymID);

    return nym.PaymentCode(accountID);
}

auto Blockchain::Imp::PaymentCodeSubaccount(
    const identifier::Nym& nymID,
    const opentxs::PaymentCode& local,
    const opentxs::PaymentCode& remote,
    const proto::HDPath path,
    const Chain chain,
    const PasswordPrompt& reason) const noexcept(false)
    -> const blockchain::PaymentCode&
{
    auto lock = Lock{nym_mutex(nymID)};
    const auto accountID =
        blockchain::internal::PaymentCode::GetID(api_, chain, local, remote);
    const auto type = api_.Storage().Bip47Chain(nymID, accountID);

    if (contact::ContactItemType::Error == type) {
        const auto id =
            new_payment_code(lock, nymID, local, remote, path, chain, reason);

        if (accountID != id) {
            throw std::out_of_range("Failed to create account");
        }
    }

    auto& balanceList = balance_lists_.Get(chain);
    auto& tree = balanceList.Nym(nymID);

    return tree.PaymentCode(accountID);
}

auto Blockchain::Imp::ProcessContact(const Contact&) const noexcept -> bool
{
    return false;
}

auto Blockchain::Imp::ProcessMergedContact(const Contact&, const Contact&)
    const noexcept -> bool
{
    return false;
}

auto Blockchain::Imp::ProcessSyncData(OTZMQMessage&&) const noexcept -> void {}

auto Blockchain::Imp::ProcessTransaction(
    const Chain,
    const Tx&,
    const PasswordPrompt&) const noexcept -> bool
{
    return false;
}

auto Blockchain::Imp::PubkeyHash(
    [[maybe_unused]] const Chain chain,
    const Data& pubkey) const noexcept(false) -> OTData
{
    if (pubkey.empty()) { throw std::runtime_error("Empty pubkey"); }

    if (COMPRESSED_PUBKEY_SIZE != pubkey.size()) {
        throw std::runtime_error("Incorrect pubkey size");
    }

    auto output = Data::Factory();

    if (false == api_.Crypto().Hash().Digest(
                     opentxs::crypto::HashType::Bitcoin,
                     pubkey.Bytes(),
                     output->WriteInto())) {
        throw std::runtime_error("Unable to calculate hash.");
    }

    return output;
}

auto Blockchain::Imp::RecipientContact(
    const blockchain::Key& key) const noexcept -> OTIdentifier
{
    static const auto blank = api_.Factory().Identifier();
    const auto& [account, subchain, index] = key;
    using Subchain = api::client::blockchain::Subchain;

    if (Subchain::Notification == subchain) { return blank; }

    const auto accountID = api_.Factory().Identifier(account);
    const auto& owner = Owner(accountID);

    try {
        if (owner.empty()) {
            throw std::runtime_error{"Failed to load account owner"};
        }

        const auto& element = GetKey(key);

        switch (subchain) {
            case Subchain::Internal:
            case Subchain::External:
            case Subchain::Incoming: {

                return contacts_.NymToContact(owner);
            }
            case Subchain::Outgoing: {

                return element.Contact();
            }
            default: {

                return blank;
            }
        }
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return blank;
    }
}

auto Blockchain::Imp::Release(const blockchain::Key key) const noexcept -> bool
{
    try {
        const auto [id, subchain, index] = key;
        const auto accountID = api_.Factory().Identifier(id);

        return get_node(accountID).Unreserve(subchain, index);
    } catch (...) {

        return false;
    }
}

auto Blockchain::Imp::Reorg() const noexcept -> const zmq::socket::Publish&
{
    OT_FAIL;
}

auto Blockchain::Imp::ReportProgress(
    const Chain,
    const opentxs::blockchain::block::Height,
    const opentxs::blockchain::block::Height) const noexcept -> void
{
}

auto Blockchain::Imp::ReportScan(
    const Chain,
    const identifier::Nym&,
    const Identifier&,
    const blockchain::Subchain,
    const opentxs::blockchain::block::Position&) const noexcept -> void
{
}

auto Blockchain::Imp::RestoreNetworks() const noexcept -> void {}

auto Blockchain::Imp::SenderContact(const blockchain::Key& key) const noexcept
    -> OTIdentifier
{
    static const auto blank = api_.Factory().Identifier();
    const auto& [account, subchain, index] = key;
    using Subchain = api::client::blockchain::Subchain;

    if (Subchain::Notification == subchain) { return blank; }

    const auto accountID = api_.Factory().Identifier(account);
    const auto& owner = Owner(accountID);

    try {
        if (owner.empty()) {
            throw std::runtime_error{"Failed to load account owner"};
        }

        const auto& element = GetKey(key);

        switch (subchain) {
            case Subchain::Internal:
            case Subchain::Outgoing: {

                return contacts_.NymToContact(owner);
            }
            case Subchain::External:
            case Subchain::Incoming: {

                return element.Contact();
            }
            default: {

                return blank;
            }
        }
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return blank;
    }
}

auto Blockchain::Imp::Shutdown() noexcept -> void {}

auto Blockchain::Imp::Start(const Chain, const std::string&) const noexcept
    -> bool
{
    return false;
}

auto Blockchain::Imp::StartSyncServer(
    const std::string&,
    const std::string&,
    const std::string&,
    const std::string&) const noexcept -> bool
{
    return false;
}

auto Blockchain::Imp::Stop(const Chain type) const noexcept -> bool
{
    return false;
}

auto Blockchain::Imp::Unconfirm(
    const blockchain::Key key,
    const opentxs::blockchain::block::Txid& tx,
    const Time time) const noexcept -> bool
{
    try {
        const auto [id, subchain, index] = key;
        const auto accountID = api_.Factory().Identifier(id);

        return get_node(accountID).Unconfirm(subchain, index, tx, time);
    } catch (...) {

        return false;
    }
}

auto Blockchain::Imp::UpdateBalance(
    const opentxs::blockchain::Type,
    const opentxs::blockchain::Balance) const noexcept -> void
{
}

auto Blockchain::Imp::UpdateBalance(
    const identifier::Nym&,
    const opentxs::blockchain::Type,
    const opentxs::blockchain::Balance) const noexcept -> void
{
}

auto Blockchain::Imp::UpdateElement(std::vector<ReadView>&) const noexcept
    -> void
{
}

auto Blockchain::Imp::UpdatePeer(
    const opentxs::blockchain::Type,
    const std::string&) const noexcept -> void
{
}

auto Blockchain::Imp::validate_nym(const identifier::Nym& nymID) const noexcept
    -> bool
{
    if (false == nymID.empty()) {
        if (0 < api_.Wallet().LocalNyms().count(nymID)) { return true; }
    }

    return false;
}
}  // namespace opentxs::api::client::implementation
