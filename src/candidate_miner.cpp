// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <candidate_miner.h>

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace mercaminer {

ScanResult MineBlockCandidate(
    NonceScanner& scanner,
    BlockCandidate& candidate,
    const UInt256& target,
    std::uint32_t nonce_begin,
    std::uint32_t nonce_end,
    const std::atomic_bool* cancelled)
{
    if (candidate.serialized_block.size() <
        BlockHeader::SERIALIZED_SIZE) {
        throw std::invalid_argument(
            "candidate block is shorter than its header");
    }

    const auto original_header =
        candidate.header.Serialize();

    if (!std::equal(
            original_header.begin(),
            original_header.end(),
            candidate.serialized_block.begin())) {
        throw std::invalid_argument(
            "candidate header does not match serialized block");
    }

    const ScanResult result =
        scanner.Scan(
            candidate.header,
            target,
            nonce_begin,
            nonce_end,
            cancelled);

    if (result.status != ScanStatus::FOUND) {
        return result;
    }

    candidate.header.nonce =
        result.nonce;

    const auto solved_header =
        candidate.header.Serialize();

    std::copy(
        solved_header.begin(),
        solved_header.end(),
        candidate.serialized_block.begin());

    return result;
}

} // namespace mercaminer
