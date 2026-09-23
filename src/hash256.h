// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_HASH256_H
#define MERCAMINER_HASH256_H

#include <uint256.h>

#include <array>
#include <span>

namespace mercaminer {

using Sha256Digest = std::array<unsigned char, 32>;

Sha256Digest Sha256(
    std::span<const unsigned char> input);

UInt256 DoubleSha256(
    std::span<const unsigned char> input);

} // namespace mercaminer

#endif // MERCAMINER_HASH256_H
