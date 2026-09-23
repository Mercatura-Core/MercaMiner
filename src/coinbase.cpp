// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <coinbase.h>

#include <hash256.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace mercaminer {
namespace {

constexpr std::uint32_t NULL_PREVOUT_INDEX{
    std::numeric_limits<std::uint32_t>::max()};

constexpr std::uint32_t COINBASE_SEQUENCE{
    0xfffffffeU};

constexpr std::uint32_t TRANSACTION_VERSION{2};

Bytes SerializePositiveScriptNum(
    std::uint32_t value)
{
    Bytes result;

    while (value != 0) {
        result.push_back(
            static_cast<unsigned char>(
                value & 0xffU));
        value >>= 8;
    }

    if (!result.empty() &&
        (result.back() & 0x80U) != 0) {
        result.push_back(0x00);
    }

    return result;
}

void AppendCoinbaseInput(
    Bytes& out,
    std::span<const unsigned char> script_sig)
{
    static constexpr std::array<unsigned char, 32>
        null_hash{};

    AppendCompactSize(out, 1);
    AppendBytes(out, null_hash);
    AppendLE32(out, NULL_PREVOUT_INDEX);

    AppendCompactSize(
        out,
        script_sig.size());
    AppendBytes(
        out,
        script_sig);

    AppendLE32(
        out,
        COINBASE_SEQUENCE);
}

void AppendOutputs(
    Bytes& out,
    std::int64_t coinbase_value,
    std::span<const unsigned char> payout_script,
    std::span<const unsigned char> witness_commitment_script)
{
    AppendCompactSize(out, 2);

    AppendLE64(
        out,
        static_cast<std::uint64_t>(
            coinbase_value));

    AppendCompactSize(
        out,
        payout_script.size());
    AppendBytes(
        out,
        payout_script);

    AppendLE64(out, 0);

    AppendCompactSize(
        out,
        witness_commitment_script.size());
    AppendBytes(
        out,
        witness_commitment_script);
}

void ValidateWitnessCommitment(
    std::span<const unsigned char> script)
{
    static constexpr std::array<unsigned char, 6>
        prefix{
            0x6a,
            0x24,
            0xaa,
            0x21,
            0xa9,
            0xed};

    if (script.size() != 38) {
        throw CoinbaseException(
            "witness commitment script must be exactly 38 bytes");
    }

    for (std::size_t i = 0;
         i < prefix.size();
         ++i) {
        if (script[i] != prefix[i]) {
            throw CoinbaseException(
                "witness commitment script has invalid prefix");
        }
    }
}

} // namespace

Bytes BuildBip34HeightPrefix(
    std::uint32_t height)
{
    if (height == 0) {
        throw CoinbaseException(
            "coinbase height must be greater than zero");
    }

    Bytes result;

    if (height <= 16) {
        result.push_back(
            static_cast<unsigned char>(
                0x50U + height));
        return result;
    }

    const Bytes encoded =
        SerializePositiveScriptNum(height);

    if (encoded.empty() ||
        encoded.size() >= 0x4c) {
        throw CoinbaseException(
            "coinbase height encoding is invalid");
    }

    result.push_back(
        static_cast<unsigned char>(
            encoded.size()));

    AppendBytes(
        result,
        encoded);

    return result;
}

CoinbaseTransaction BuildCoinbaseTransaction(
    std::uint32_t height,
    std::int64_t coinbase_value,
    std::span<const unsigned char> payout_script,
    std::span<const unsigned char> coinbase_aux_flags,
    std::span<const unsigned char> extranonce,
    std::span<const unsigned char> witness_commitment_script)
{
    if (height == 0) {
        throw CoinbaseException(
            "coinbase height must be greater than zero");
    }

    if (coinbase_value < 0) {
        throw CoinbaseException(
            "coinbase value must not be negative");
    }

    ValidateWitnessCommitment(
        witness_commitment_script);

    Bytes script_sig =
        BuildBip34HeightPrefix(height);

    AppendBytes(
        script_sig,
        coinbase_aux_flags);

    AppendBytes(
        script_sig,
        extranonce);

    if (script_sig.size() < 2 ||
        script_sig.size() > 100) {
        throw CoinbaseException(
            "coinbase scriptSig must be between 2 and 100 bytes");
    }

    CoinbaseTransaction result;
    result.script_sig = script_sig;

    Bytes stripped;

    AppendLE32(
        stripped,
        TRANSACTION_VERSION);

    AppendCoinbaseInput(
        stripped,
        script_sig);

    AppendOutputs(
        stripped,
        coinbase_value,
        payout_script,
        witness_commitment_script);

    AppendLE32(
        stripped,
        height - 1);

    Bytes with_witness;

    AppendLE32(
        with_witness,
        TRANSACTION_VERSION);

    // BIP144 marker and witness flag.
    with_witness.push_back(0x00);
    with_witness.push_back(0x01);

    AppendCoinbaseInput(
        with_witness,
        script_sig);

    AppendOutputs(
        with_witness,
        coinbase_value,
        payout_script,
        witness_commitment_script);

    // Coinbase witness:
    // one stack item containing the 32-byte zero reserved value.
    AppendCompactSize(
        with_witness,
        1);

    AppendCompactSize(
        with_witness,
        32);

    static constexpr std::array<unsigned char, 32>
        witness_reserved_value{};

    AppendBytes(
        with_witness,
        witness_reserved_value);

    AppendLE32(
        with_witness,
        height - 1);

    result.serialized_without_witness =
        std::move(stripped);

    result.serialized_with_witness =
        std::move(with_witness);

    result.txid =
        DoubleSha256(
            result.serialized_without_witness);

    result.wtxid =
        DoubleSha256(
            result.serialized_with_witness);

    return result;
}

} // namespace mercaminer
