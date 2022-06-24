// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Helpers.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <stdexcept>

namespace ottest
{
PaymentCodeFixture PC_Fixture_Base::user_1_{};
PaymentCodeFixture PC_Fixture_Base::user_2_{};

auto PaymentCodeFixture::bip44_path(
    const ot::api::session::Client& api,
    const ot::blockchain::Type chain,
    ot::AllocateOutput destination) const -> bool
{
    if (false == seed_.has_value()) {
        throw std::runtime_error("missing seed");
    }

    if (false == api.Crypto().Blockchain().Bip44Path(
                     chain, seed_.value(), destination)) {
        throw std::runtime_error("missing path");
    }

    return true;
}

auto PaymentCodeFixture::blinding_key_public()
    -> const ot::crypto::key::EllipticCurve&
{
    if (false == bool(blind_key_secret_)) {
        throw std::runtime_error("missing private key");
    }

    auto& var = blind_key_public_;

    if (false == bool(var)) { var = blind_key_secret_->asPublicEC(); }

    if (false == bool(var)) { throw std::runtime_error("Failed"); }

    return *var;
}

auto PaymentCodeFixture::blinding_key_secret(
    const ot::api::session::Client& api,
    const ot::blockchain::Type chain,
    const ot::PasswordPrompt& reason) -> const ot::crypto::key::EllipticCurve&
{
    auto& var = blind_key_secret_;

    if (false == bool(var)) {
        auto bytes = ot::Space{};
        if (false == bip44_path(api, chain, ot::writer(bytes))) {
            throw std::runtime_error("Failed");
        }
        var = api.Crypto().Seed().AccountChildKey(
            ot::reader(bytes), ot::INTERNAL_CHAIN, index_, reason);
    }

    if (false == bool(var)) { throw std::runtime_error("Failed"); }

    return *var;
}

auto PaymentCodeFixture::blinding_key_secret(
    const ot::api::session::Client& api,
    const ot::UnallocatedCString& privateKey,
    const ot::PasswordPrompt& reason) -> const ot::crypto::key::EllipticCurve&
{
    auto& var = blind_key_secret_;

    if (false == bool(var)) {
        const auto decoded = api.Factory().DataFromHex(privateKey);
        const auto key = api.Factory().SecretFromBytes(decoded.Bytes());
        var = api.Crypto().Asymmetric().InstantiateSecp256k1Key(key, reason);
    }

    if (false == bool(var)) { throw std::runtime_error("Failed"); }

    return *var;
}

auto PaymentCodeFixture::cleanup() -> void
{
    blind_key_public_.reset();
    blind_key_secret_.reset();
    pc_public_ = std::nullopt;
    pc_secret_ = std::nullopt;
    seed_ = std::nullopt;
}

auto PaymentCodeFixture::payment_code_public(
    const ot::api::Session& api,
    const ot::UnallocatedCString& base58) -> const ot::PaymentCode&
{
    auto& var = pc_public_;

    if (false == var.has_value()) {
        var.emplace(api.Factory().PaymentCode(base58));
    }

    if (false == var.has_value()) { throw std::runtime_error("Failed"); }

    return var.value();
}

auto PaymentCodeFixture::payment_code_secret(
    const ot::api::Session& api,
    const std::uint8_t version,
    const ot::PasswordPrompt& reason) -> const ot::PaymentCode&
{
    if (false == seed_.has_value()) {
        throw std::runtime_error("missing seed");
    }

    auto& var = pc_secret_;

    if (false == var.has_value()) {
        var.emplace(api.Factory().PaymentCode(
            seed_.value(), account_, version, reason));
    }

    if (false == var.has_value()) { throw std::runtime_error("Failed"); }

    return var.value();
}

auto PaymentCodeFixture::seed(
    const ot::api::Session& api,
    const std::string_view wordList,
    const ot::PasswordPrompt& reason) -> const ot::UnallocatedCString&
{
    auto& var = seed_;

    if (false == var.has_value()) {
        const auto words = api.Factory().SecretFromText(wordList);
        const auto phrase = api.Factory().Secret(0);
        var.emplace(api.Crypto().Seed().ImportSeed(
            words,
            phrase,
            ot::crypto::SeedStyle::BIP39,
            ot::crypto::Language::en,
            reason));
    }

    if (false == var.has_value()) { throw std::runtime_error("Failed"); }

    return var.value();
}

PC_Fixture_Base::PC_Fixture_Base(
    const std::uint8_t aliceVersion,
    const std::uint8_t bobVersion,
    const ot::UnallocatedCString& aliceBip39,
    const ot::UnallocatedCString& bobBip39,
    const ot::UnallocatedCString& aliceExpectedPC,
    const ot::UnallocatedCString& bobExpectedPC) noexcept
    : api_(ot::Context().StartClientSession(0))
    , reason_(api_.Factory().PasswordPrompt(__func__))
    , alice_seed_(user_1_.seed(api_, aliceBip39, reason_))
    , bob_seed_(user_2_.seed(api_, bobBip39, reason_))
    , alice_pc_secret_(user_1_.payment_code_secret(api_, aliceVersion, reason_))
    , alice_pc_public_(user_1_.payment_code_public(api_, aliceExpectedPC))
    , bob_pc_secret_(user_2_.payment_code_secret(api_, bobVersion, reason_))
    , bob_pc_public_(user_2_.payment_code_public(api_, bobExpectedPC))
{
}

auto PC_Fixture_Base::Shutdown() noexcept -> void
{
    user_1_.cleanup();
    user_2_.cleanup();
}
}  // namespace ottest
