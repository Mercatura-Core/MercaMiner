// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_CANDIDATE_MINER_H
#define MERCAMINER_CANDIDATE_MINER_H

#include <block_builder.h>
#include <scanner.h>

#include <atomic>
#include <cstdint>

namespace mercaminer {

ScanResult MineBlockCandidate(
    NonceScanner& scanner,
    BlockCandidate& candidate,
    const UInt256& target,
    std::uint32_t nonce_begin,
    std::uint32_t nonce_end,
    const std::atomic_bool* cancelled = nullptr);

} // namespace mercaminer

#endif // MERCAMINER_CANDIDATE_MINER_H
