// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <block_builder.h>

#include <merkle.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mercaminer {
namespace {

unsigned char HexDigit(
    char value,
    std::string_view field)
{
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned char>(
            value - '0');
    }

    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned char>(
            value - 'a' + 10);
    }

    if (value >= 'A' && value <= 'F') {
        return static_cast<unsigned char>(
            value - 'A' + 10);
    }

    throw BlockBuildException(
        std::string{field} +
        " contains non-hex characters");
}

Bytes DecodeHex(
    std::string_view hex,
    std::string_view field)
{
    if ((hex.size() & 1U) != 0) {
        throw BlockBuildException(
            std::string{field} +
            " must contain an even number of hex digits");
    }

    Bytes result;
    result.reserve(hex.size() / 2);

    for (std::size_t i = 0;
         i < hex.size();
         i += 2) {
        const unsigned char high =
            HexDigit(hex[i], field);

        const unsigned char low =
            HexDigit(hex[i + 1], field);

        result.push_back(
            static_cast<unsigned char>(
                (high << 4) | low));
    }

    return result;
}

} // namespace

BlockCandidate BuildBlockCandidate(
    const BlockTemplate& block_template,
    std::span<const unsigned char> payout_script,
    std::span<const unsigned char> extranonce)
{
    if (block_template.height == 0 ||
        block_template.height >
            std::numeric_limits<std::uint32_t>::max()) {
        throw BlockBuildException(
            "template height is outside the supported range");
    }

    if (block_template.coinbase_value >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw BlockBuildException(
            "template coinbase value exceeds int64 range");
    }

    const Bytes coinbase_aux =
        DecodeHex(
            block_template.coinbase_aux_flags,
            "coinbaseaux.flags");

    const Bytes witness_commitment =
        DecodeHex(
            block_template.witness_commitment_hex,
            "default_witness_commitment");

    BlockCandidate candidate;

    candidate.coinbase =
        BuildCoinbaseTransaction(
            static_cast<std::uint32_t>(
                block_template.height),
            static_cast<std::int64_t>(
                block_template.coinbase_value),
            payout_script,
            coinbase_aux,
            extranonce,
            witness_commitment);

    std::vector<UInt256> merkle_leaves;

    merkle_leaves.reserve(
        1 + block_template.transactions.size());

    merkle_leaves.push_back(
        candidate.coinbase.txid);

    for (const auto& transaction :
         block_template.transactions) {
        merkle_leaves.push_back(
            transaction.txid);
    }

    const MerkleResult merkle =
        ComputeMerkleRoot(
            std::move(merkle_leaves));

    if (merkle.mutated) {
        throw BlockBuildException(
            "template transaction set has a mutated merkle tree");
    }

    candidate.header.version =
        block_template.version;

    candidate.header.previous_block =
        block_template.previous_block_hash;

    candidate.header.merkle_root =
        merkle.root;

    candidate.header.time =
        block_template.current_time;

    candidate.header.bits =
        block_template.bits;

    candidate.header.nonce =
        block_template.nonce_min;

    const auto serialized_header =
        candidate.header.Serialize();

    AppendBytes(
        candidate.serialized_block,
        serialized_header);

    AppendCompactSize(
        candidate.serialized_block,
        static_cast<std::uint64_t>(
            1 + block_template.transactions.size()));

    AppendBytes(
        candidate.serialized_block,
        candidate.coinbase.serialized_with_witness);

    for (const auto& transaction :
         block_template.transactions) {
        const Bytes raw_transaction =
            DecodeHex(
                transaction.data_hex,
                "template transaction data");

        AppendBytes(
            candidate.serialized_block,
            raw_transaction);
    }

    if (candidate.serialized_block.size() >
        block_template.size_limit) {
        throw BlockBuildException(
            "candidate block exceeds template serialized-byte limit");
    }

    return candidate;
}

} // namespace mercaminer
