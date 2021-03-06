// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/p256_key_exchange.h"

#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>

#include "base/logging.h"

namespace net {

P256KeyExchange::P256KeyExchange(EC_KEY* private_key, const uint8* public_key)
    : private_key_(private_key) {
  memcpy(public_key_, public_key, sizeof(public_key_));
}

P256KeyExchange::~P256KeyExchange() {
}

// static
P256KeyExchange* P256KeyExchange::New(base::StringPiece key) {
  if (key.empty()) {
    DLOG(INFO) << "Private key is empty";
    return NULL;
  }

  const uint8* keyp = reinterpret_cast<const uint8*>(key.data());
  crypto::ScopedOpenSSL<EC_KEY, EC_KEY_free> private_key(
      d2i_ECPrivateKey(NULL, &keyp, key.size()));
  if (!private_key.get() || !EC_KEY_check_key(private_key.get())) {
    DLOG(INFO) << "Private key is invalid";
    return NULL;
  }

  uint8 public_key[kUncompressedP256PointBytes];
  if (EC_POINT_point2oct(
          EC_KEY_get0_group(private_key.get()),
          EC_KEY_get0_public_key(private_key.get()),
          POINT_CONVERSION_UNCOMPRESSED,
          public_key,
          sizeof(public_key),
          NULL) != sizeof(public_key)) {
    DLOG(INFO) << "Can't get public key";
    return NULL;
  }

  return new P256KeyExchange(private_key.release(), public_key);
}

// static
std::string P256KeyExchange::NewPrivateKey() {
  crypto::ScopedOpenSSL<EC_KEY, EC_KEY_free> key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (!key.get() || !EC_KEY_generate_key(key.get())) {
    DLOG(INFO) << "Can't generate a new private key";
    return std::string();
  }

  int key_len = i2d_ECPrivateKey(key.get(), NULL);
  if (key_len <= 0) {
    DLOG(INFO) << "Can't convert private key to string";
    return std::string();
  }
  scoped_ptr<uint8[]> private_key(new uint8[key_len]);
  uint8* keyp = private_key.get();
  if (!i2d_ECPrivateKey(key.get(), &keyp)) {
    DLOG(INFO) << "Can't convert private key to string";
    return std::string();
  }
  return std::string(reinterpret_cast<char*>(private_key.get()), key_len);
}

bool P256KeyExchange::CalculateSharedKey(
    const base::StringPiece& peer_public_value,
    std::string* out_result) const {
  if (peer_public_value.size() != kUncompressedP256PointBytes) {
    DLOG(INFO) << "Peer public value is invalid";
    return false;
  }

  crypto::ScopedOpenSSL<EC_POINT, EC_POINT_free> point(
      EC_POINT_new(EC_KEY_get0_group(private_key_.get())));
  if (!point.get() ||
      !EC_POINT_oct2point( /* also test if point is on curve */
          EC_KEY_get0_group(private_key_.get()),
          point.get(),
          reinterpret_cast<const uint8*>(peer_public_value.data()),
          peer_public_value.size(),
          NULL)) {
    DLOG(INFO) << "Can't convert peer public value to curve point";
    return false;
  }

  uint8 result[kP256FieldBytes];
  if (ECDH_compute_key(
      result,
      sizeof(result),
      point.get(),
      private_key_.get(),
      NULL) != sizeof(result)) {
    DLOG(INFO) << "Can't compute ECDH shared key";
    return false;
  }

  out_result->assign(reinterpret_cast<char*>(result), sizeof(result));
  return true;
}

base::StringPiece P256KeyExchange::public_value() const {
  return base::StringPiece(reinterpret_cast<const char*>(public_key_),
                           sizeof(public_key_));
}

CryptoTag P256KeyExchange::tag() const {
  return kP256;
}

}  // namespace net

