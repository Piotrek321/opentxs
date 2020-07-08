// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                  // IWYU pragma: associated
#include "1_Internal.hpp"                // IWYU pragma: associated
#include "crypto/key/EllipticCurve.hpp"  // IWYU pragma: associated

#include <stdexcept>
#include <utility>

#include "crypto/key/Asymmetric.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/core/Secret.hpp"
#include "opentxs/crypto/key/EllipticCurve.hpp"
#include "opentxs/crypto/library/EcdsaProvider.hpp"
#include "opentxs/protobuf/AsymmetricKey.pb.h"
#include "opentxs/protobuf/Ciphertext.pb.h"
#include "opentxs/protobuf/Enums.pb.h"

#define OT_METHOD "opentxs::crypto::key::implementation::EllipticCurve::"

namespace opentxs::crypto::key
{
const VersionNumber EllipticCurve::DefaultVersion{2};
const VersionNumber EllipticCurve::MaxVersion{2};
}  // namespace opentxs::crypto::key

namespace opentxs::crypto::key::implementation
{
EllipticCurve::EllipticCurve(
    const api::internal::Core& api,
    const crypto::EcdsaProvider& ecdsa,
    const proto::AsymmetricKey& serialized) noexcept(false)
    : Asymmetric(
          api,
          ecdsa,
          serialized,
          [&](auto& pubkey, auto&) -> EncryptedKey {
              return extract_key(api, ecdsa, serialized, pubkey);
          })
    , ecdsa_(ecdsa)
{
}

EllipticCurve::EllipticCurve(
    const api::internal::Core& api,
    const crypto::EcdsaProvider& ecdsa,
    const proto::AsymmetricKeyType keyType,
    const proto::KeyRole role,
    const VersionNumber version,
    const PasswordPrompt& reason) noexcept(false)
    : Asymmetric(
          api,
          ecdsa,
          keyType,
          role,
          version,
          [&](auto& pub, auto& prv) -> EncryptedKey {
              return create_key(
                  api,
                  ecdsa,
                  {},
                  role,
                  pub.WriteInto(),
                  prv.WriteInto(Secret::Mode::Mem),
                  prv,
                  {},
                  reason);
          })
    , ecdsa_(ecdsa)
{
    if (false == bool(encrypted_key_)) {
        throw std::runtime_error("Failed to instantiate encrypted_key_");
    }
}

#if OT_CRYPTO_WITH_BIP32
EllipticCurve::EllipticCurve(
    const api::internal::Core& api,
    const crypto::EcdsaProvider& ecdsa,
    const proto::AsymmetricKeyType keyType,
    const Secret& privateKey,
    const Data& publicKey,
    const proto::KeyRole role,
    const VersionNumber version,
    key::Symmetric& sessionKey,
    const PasswordPrompt& reason) noexcept(false)
    : Asymmetric(
          api,
          ecdsa,
          keyType,
          role,
          true,
          true,
          version,
          OTData{publicKey},
          [&](auto&, auto&) -> EncryptedKey {
              return encrypt_key(sessionKey, reason, true, privateKey.Bytes());
          })
    , ecdsa_(ecdsa)
{
    if (false == bool(encrypted_key_)) {
        throw std::runtime_error("Failed to instantiate encrypted_key_");
    }
}
#endif  // OT_CRYPTO_WITH_BIP32

EllipticCurve::EllipticCurve(const EllipticCurve& rhs) noexcept
    : Asymmetric(rhs)
    , ecdsa_(rhs.ecdsa_)
{
}

auto EllipticCurve::asPublicEC() const noexcept
    -> std::unique_ptr<key::EllipticCurve>
{
    auto output = std::unique_ptr<EllipticCurve>{clone_ec()};

    OT_ASSERT(output);

    auto& copy = *output;
    copy.erase_private_data();

    OT_ASSERT(false == copy.HasPrivate());

    return std::move(output);
}

auto EllipticCurve::extract_key(
    const api::internal::Core& api,
    const crypto::EcdsaProvider& ecdsa,
    const proto::AsymmetricKey& proto,
    Data& publicKey) -> std::unique_ptr<proto::Ciphertext>
{
    auto output = std::unique_ptr<proto::Ciphertext>{};
    publicKey.Assign(proto.key());

    if ((proto::KEYMODE_PRIVATE == proto.mode()) && proto.has_encryptedkey()) {
        output = std::make_unique<proto::Ciphertext>(proto.encryptedkey());

        OT_ASSERT(output);
    }

    return output;
}

auto EllipticCurve::serialize_public(EllipticCurve* in)
    -> std::shared_ptr<proto::AsymmetricKey>
{
    std::unique_ptr<EllipticCurve> copy{in};

    OT_ASSERT(copy);

    copy->erase_private_data();

    return copy->Serialize();
}

auto EllipticCurve::SignDER(
    const ReadView preimage,
    const proto::HashType hash,
    Space& output,
    const PasswordPrompt& reason) const noexcept -> bool
{
    if (false == has_private_) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Missing private key").Flush();

        return false;
    }

    bool success = ecdsa_.SignDER(api_, preimage, *this, hash, output, reason);

    if (false == success) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to sign preimage").Flush();
    }

    return success;
}
}  // namespace opentxs::crypto::key::implementation
