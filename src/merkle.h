// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_MERKLE_H
#define MERCAMINER_MERKLE_H

#include <uint256.h>

#include <vector>

namespace mercaminer {

struct MerkleResult
{
    UInt256 root{};
    bool mutated{false};
};

MerkleResult ComputeMerkleRoot(
    std::vector<UInt256> hashes);

} // namespace mercaminer

#endif // MERCAMINER_MERKLE_H
