// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <hash256.h>

#include <crypto/sha256.h>

namespace mercaminer {

Sha256Digest Sha256(
    std::span<const unsigned char> input)
{
    Sha256Digest output{};

    CSHA256 hasher;
    if (!input.empty()) {
        hasher.Write(
            input.data(),
            input.size());
    }
    hasher.Finalize(
        output.data());

    return output;
}

UInt256 DoubleSha256(
    std::span<const unsigned char> input)
{
    Sha256Digest first{};
    Sha256Digest second{};

    CSHA256{}
        .Write(
            input.data(),
            input.size())
        .Finalize(
            first.data());

    CSHA256{}
        .Write(
            first.data(),
            first.size())
        .Finalize(
            second.data());

    return UInt256{second};
}

} // namespace mercaminer
