// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_RUNTIME_REPORTER_H
#define MERCAMINER_RUNTIME_REPORTER_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <thread>

namespace mercaminer {

struct MiningRuntimeStats
{
    std::atomic<std::uint64_t> completed_hashes{0};
    std::atomic<std::uint64_t> accepted_blocks{0};
    std::atomic<std::uint64_t> stale_work{0};
    std::atomic<std::uint64_t> rejected_blocks{0};
    std::atomic<std::uint64_t> current_height{0};
};

struct MiningRuntimeSnapshot
{
    std::uint64_t completed_hashes{};
    std::uint64_t accepted_blocks{};
    std::uint64_t stale_work{};
    std::uint64_t rejected_blocks{};
    std::uint64_t current_height{};
};

MiningRuntimeSnapshot SnapshotMiningRuntimeStats(
    const MiningRuntimeStats& stats) noexcept;

double CalculateHashRate(
    std::uint64_t previous_hashes,
    std::uint64_t current_hashes,
    std::chrono::steady_clock::duration elapsed) noexcept;

std::string FormatMiningUptime(
    std::chrono::steady_clock::duration elapsed);

class RuntimeReporter
{
public:
    RuntimeReporter(
        const MiningRuntimeStats& stats,
        std::size_t worker_count,
        std::chrono::milliseconds interval,
        std::ostream& output);

    RuntimeReporter(const RuntimeReporter&) = delete;
    RuntimeReporter& operator=(const RuntimeReporter&) = delete;

    void Stop() noexcept;

private:
    std::jthread m_thread;
};

} // namespace mercaminer

#endif // MERCAMINER_RUNTIME_REPORTER_H
