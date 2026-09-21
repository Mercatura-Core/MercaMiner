// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_PARALLEL_MINER_H
#define MERCAMINER_PARALLEL_MINER_H

#include <block_builder.h>
#include <scanner.h>
#include <uint256.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mercaminer {

struct NonceRange
{
    std::uint32_t begin{};
    std::uint32_t end{};
};

std::vector<NonceRange> PartitionNonceRange(
    std::uint32_t nonce_begin,
    std::uint32_t nonce_end,
    std::size_t worker_count);

struct ParallelMineResult
{
    ScanStatus status{ScanStatus::EXHAUSTED};
    std::uint32_t nonce{};
    UInt256 hash{};
    std::uint64_t hashes_checked{};
    std::optional<std::size_t> winning_worker;
    std::size_t active_workers{};
};

class ParallelCandidateMiner
{
public:
    explicit ParallelCandidateMiner(
        std::size_t worker_count);

    std::size_t WorkerCount() const noexcept
    {
        return m_scanners.size();
    }

    std::size_t ScratchpadBytes() const noexcept;

    ParallelMineResult Mine(
        BlockCandidate& candidate,
        const UInt256& target,
        std::uint32_t nonce_begin,
        std::uint32_t nonce_end,
        const std::atomic_bool* cancelled = nullptr,
        const std::atomic_bool* cancelled_secondary = nullptr,
        std::atomic<std::uint64_t>* live_hashes = nullptr);

private:
    std::vector<std::unique_ptr<NonceScanner>>
        m_scanners;
};

} // namespace mercaminer

#endif // MERCAMINER_PARALLEL_MINER_H
