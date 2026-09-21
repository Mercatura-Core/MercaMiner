// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_BLOCK_BUILDER_H
#define MERCAMINER_BLOCK_BUILDER_H

#include <coinbase.h>
#include <gbt.h>
#include <header.h>
#include <serialization.h>

#include <span>
#include <stdexcept>

namespace mercaminer {

class BlockBuildException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct BlockCandidate
{
    BlockHeader header;
    CoinbaseTransaction coinbase;
    Bytes serialized_block;
};

BlockCandidate BuildBlockCandidate(
    const BlockTemplate& block_template,
    std::span<const unsigned char> payout_script,
    std::span<const unsigned char> extranonce);

} // namespace mercaminer

#endif // MERCAMINER_BLOCK_BUILDER_H
