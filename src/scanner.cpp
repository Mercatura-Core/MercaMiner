// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <scanner.h>

#include <crypto/mercahash.h>
#include <target.h>

#include <array>
#include <stdexcept>

namespace mercaminer {

NonceScanner::NonceScanner()
    : m_scratchpad(mercahash::SCRATCHPAD_BYTES)
{
}

ScanResult NonceScanner::Scan(
    const BlockHeader& header,
    const UInt256& target,
    std::uint32_t nonce_begin,
    std::uint32_t nonce_end,
    const std::atomic_bool* cancelled)
{
    if (nonce_begin > nonce_end) {
        throw std::invalid_argument(
            "nonce range begin exceeds nonce range end");
    }

    BlockHeader candidate_header = header;
    std::uint32_t nonce = nonce_begin;
    std::uint64_t hashes_checked = 0;

    for (;;) {
        if (cancelled != nullptr &&
            cancelled->load(std::memory_order_relaxed)) {
            return ScanResult{
                ScanStatus::CANCELLED,
                nonce,
                UInt256{},
                hashes_checked};
        }

        candidate_header.nonce = nonce;

        const auto serialized =
            candidate_header.Serialize();

        std::array<unsigned char, mercahash::OUTPUT_SIZE>
            output{};

        mercahash::HashV1(
            serialized,
            m_scratchpad,
            output);

        ++hashes_checked;

        const UInt256 hash{output};

        if (MeetsTarget(hash, target)) {
            return ScanResult{
                ScanStatus::FOUND,
                nonce,
                hash,
                hashes_checked};
        }

        if (nonce == nonce_end) {
            return ScanResult{
                ScanStatus::EXHAUSTED,
                nonce,
                hash,
                hashes_checked};
        }

        ++nonce;
    }
}

} // namespace mercaminer
