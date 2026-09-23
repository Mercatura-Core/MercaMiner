// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_COINBASE_H
#define MERCAMINER_COINBASE_H

#include <serialization.h>
#include <uint256.h>

#include <cstdint>
#include <span>
#include <stdexcept>

namespace mercaminer {

class CoinbaseException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct CoinbaseTransaction
{
    Bytes script_sig;
    Bytes serialized_without_witness;
    Bytes serialized_with_witness;
    UInt256 txid{};
    UInt256 wtxid{};
};

Bytes BuildBip34HeightPrefix(
    std::uint32_t height);

CoinbaseTransaction BuildCoinbaseTransaction(
    std::uint32_t height,
    std::int64_t coinbase_value,
    std::span<const unsigned char> payout_script,
    std::span<const unsigned char> coinbase_aux_flags,
    std::span<const unsigned char> extranonce,
    std::span<const unsigned char> witness_commitment_script);

} // namespace mercaminer

#endif // MERCAMINER_COINBASE_H
