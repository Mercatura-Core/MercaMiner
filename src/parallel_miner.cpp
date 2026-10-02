// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <parallel_miner.h>

#include <candidate_miner.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace mercaminer {
namespace {

std::uint64_t SaturatingAdd(
    std::uint64_t left,
    std::uint64_t right)
{
    if (right >
        std::numeric_limits<std::uint64_t>::max() -
            left) {
        return
            std::numeric_limits<std::uint64_t>::max();
    }

    return left + right;
}

} // namespace

std::vector<NonceRange> PartitionNonceRange(
    std::uint32_t nonce_begin,
    std::uint32_t nonce_end,
    std::size_t worker_count)
{
    if (worker_count == 0) {
        throw std::invalid_argument(
            "worker count must be greater than zero");
    }

    if (nonce_begin > nonce_end) {
        throw std::invalid_argument(
            "nonce range begin exceeds nonce range end");
    }

    const std::uint64_t nonce_count =
        static_cast<std::uint64_t>(nonce_end) -
        static_cast<std::uint64_t>(nonce_begin) +
        1;

    const std::uint64_t active_count =
        std::min<std::uint64_t>(
            nonce_count,
            worker_count);

    const std::uint64_t base_size =
        nonce_count / active_count;

    const std::uint64_t remainder =
        nonce_count % active_count;

    std::vector<NonceRange> ranges;
    ranges.reserve(
        static_cast<std::size_t>(active_count));

    std::uint64_t cursor = nonce_begin;

    for (std::uint64_t worker = 0;
         worker < active_count;
         ++worker) {
        const std::uint64_t range_size =
            base_size +
            (worker < remainder ? 1 : 0);

        const std::uint64_t range_end =
            cursor + range_size - 1;

        ranges.push_back(
            NonceRange{
                static_cast<std::uint32_t>(cursor),
                static_cast<std::uint32_t>(range_end)});

        cursor = range_end + 1;
    }

    return ranges;
}

ParallelCandidateMiner::ParallelCandidateMiner(
    std::size_t worker_count)
{
    if (worker_count == 0) {
        throw std::invalid_argument(
            "worker count must be greater than zero");
    }

    m_scanners.reserve(worker_count);

    for (std::size_t i = 0;
         i < worker_count;
         ++i) {
        m_scanners.push_back(
            std::make_unique<NonceScanner>());
    }
}

std::size_t
ParallelCandidateMiner::ScratchpadBytes() const noexcept
{
    std::size_t total{0};

    for (const auto& scanner : m_scanners) {
        const std::size_t bytes =
            scanner->ScratchpadSize();

        if (bytes >
            std::numeric_limits<std::size_t>::max() -
                total) {
            return
                std::numeric_limits<std::size_t>::max();
        }

        total += bytes;
    }

    return total;
}

ParallelMineResult ParallelCandidateMiner::Mine(
    BlockCandidate& candidate,
    const UInt256& target,
    std::uint32_t nonce_begin,
    std::uint32_t nonce_end,
    const std::atomic_bool* cancelled,
    const std::atomic_bool* cancelled_secondary,
    std::atomic<std::uint64_t>* live_hashes,
    const std::atomic_bool* refresh_requested)
{
    ValidateBlockCandidateForMining(candidate);

    const auto ranges =
        PartitionNonceRange(
            nonce_begin,
            nonce_end,
            m_scanners.size());

    const std::size_t active_workers =
        ranges.size();

    std::vector<ScanResult> results(
        active_workers);

    std::atomic_bool stop{false};

    std::vector<std::jthread> threads;
    threads.reserve(active_workers);

    constexpr std::uint64_t
        SOFT_REFRESH_CHUNK_NONCES{8};

    for (std::size_t worker = 0;
         worker < active_workers;
         ++worker) {
        threads.emplace_back(
            [&, worker] {
                const auto& range =
                    ranges[worker];

                std::uint32_t chunk_begin =
                    range.begin;
                std::uint64_t worker_hashes{0};

                for (;;) {
                    if (refresh_requested != nullptr &&
                        refresh_requested->load(
                            std::memory_order_relaxed)) {
                        results[worker] =
                            ScanResult{
                                ScanStatus::CANCELLED,
                                chunk_begin,
                                UInt256{},
                                worker_hashes};
                        return;
                    }

                    const std::uint64_t proposed_end =
                        static_cast<std::uint64_t>(
                            chunk_begin) +
                        SOFT_REFRESH_CHUNK_NONCES - 1;

                    const std::uint32_t chunk_end =
                        proposed_end <
                            static_cast<std::uint64_t>(
                                range.end)
                            ? static_cast<std::uint32_t>(
                                  proposed_end)
                            : range.end;

                    ScanResult chunk_result =
                        m_scanners[worker]->Scan(
                            candidate.header,
                            target,
                            chunk_begin,
                            chunk_end,
                            &stop,
                            cancelled,
                            cancelled_secondary,
                            live_hashes);

                    worker_hashes =
                        SaturatingAdd(
                            worker_hashes,
                            chunk_result.hashes_checked);

                    chunk_result.hashes_checked =
                        worker_hashes;

                    if (chunk_result.status !=
                        ScanStatus::EXHAUSTED) {
                        results[worker] =
                            chunk_result;

                        if (chunk_result.status ==
                            ScanStatus::FOUND) {
                            stop.store(
                                true,
                                std::memory_order_relaxed);
                        }

                        return;
                    }

                    if (chunk_end == range.end) {
                        results[worker] =
                            chunk_result;
                        return;
                    }

                    if (refresh_requested != nullptr &&
                        refresh_requested->load(
                            std::memory_order_relaxed)) {
                        results[worker] =
                            ScanResult{
                                ScanStatus::CANCELLED,
                                chunk_end,
                                UInt256{},
                                worker_hashes};
                        return;
                    }

                    chunk_begin =
                        chunk_end + 1;
                }
            });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::uint64_t total_hashes{0};

    for (const auto& result : results) {
        total_hashes =
            SaturatingAdd(
                total_hashes,
                result.hashes_checked);
    }

    if ((cancelled != nullptr &&
         cancelled->load(
             std::memory_order_relaxed)) ||
        (cancelled_secondary != nullptr &&
         cancelled_secondary->load(
             std::memory_order_relaxed))) {
        return ParallelMineResult{
            ScanStatus::CANCELLED,
            0,
            UInt256{},
            total_hashes,
            std::nullopt,
            active_workers};
    }

    std::optional<std::size_t> winner;

    for (std::size_t worker = 0;
         worker < results.size();
         ++worker) {
        if (results[worker].status !=
            ScanStatus::FOUND) {
            continue;
        }

        if (!winner.has_value() ||
            results[worker].nonce <
                results[*winner].nonce) {
            winner = worker;
        }
    }

    if (winner.has_value()) {
        const auto& result =
            results[*winner];

        ApplySolvedNonce(
            candidate,
            result.nonce);

        return ParallelMineResult{
            ScanStatus::FOUND,
            result.nonce,
            result.hash,
            total_hashes,
            winner,
            active_workers};
    }

    for (const auto& result : results) {
        if (result.status ==
            ScanStatus::CANCELLED) {
            return ParallelMineResult{
                ScanStatus::CANCELLED,
                0,
                UInt256{},
                total_hashes,
                std::nullopt,
                active_workers};
        }
    }

    return ParallelMineResult{
        ScanStatus::EXHAUSTED,
        nonce_end,
        UInt256{},
        total_hashes,
        std::nullopt,
        active_workers};
}

} // namespace mercaminer
