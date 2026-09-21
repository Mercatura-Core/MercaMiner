// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <runtime_reporter.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <syncstream>
#include <thread>

namespace mercaminer {
namespace {

bool WaitForInterval(
    std::stop_token token,
    std::chrono::milliseconds interval)
{
    using namespace std::chrono_literals;

    const auto deadline =
        std::chrono::steady_clock::now() +
        interval;

    while (!token.stop_requested()) {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= deadline) {
            return true;
        }

        auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    deadline - now);

        if (remaining <=
            std::chrono::milliseconds{0}) {
            remaining =
                std::chrono::milliseconds{1};
        }

        const auto delay =
            std::min(
                remaining,
                std::chrono::milliseconds{100});

        std::this_thread::sleep_for(delay);
    }

    return false;
}

} // namespace

MiningRuntimeSnapshot SnapshotMiningRuntimeStats(
    const MiningRuntimeStats& stats) noexcept
{
    return MiningRuntimeSnapshot{
        stats.completed_hashes.load(
            std::memory_order_relaxed),
        stats.accepted_blocks.load(
            std::memory_order_relaxed),
        stats.stale_work.load(
            std::memory_order_relaxed),
        stats.rejected_blocks.load(
            std::memory_order_relaxed),
        stats.current_height.load(
            std::memory_order_relaxed)};
}

double CalculateHashRate(
    std::uint64_t previous_hashes,
    std::uint64_t current_hashes,
    std::chrono::steady_clock::duration elapsed) noexcept
{
    if (current_hashes < previous_hashes ||
        elapsed <=
            std::chrono::steady_clock::duration::zero()) {
        return 0.0;
    }

    const double seconds =
        std::chrono::duration<double>(
            elapsed).count();

    if (seconds <= 0.0) {
        return 0.0;
    }

    return static_cast<double>(
               current_hashes -
               previous_hashes) /
        seconds;
}

std::string FormatMiningUptime(
    std::chrono::steady_clock::duration elapsed)
{
    auto total_seconds =
        std::chrono::duration_cast<
            std::chrono::seconds>(
                elapsed).count();

    if (total_seconds < 0) {
        total_seconds = 0;
    }

    const auto hours =
        total_seconds / 3600;

    const auto minutes =
        (total_seconds % 3600) / 60;

    const auto seconds =
        total_seconds % 60;

    std::ostringstream out;

    out
        << std::setfill('0')
        << std::setw(2)
        << hours
        << ':'
        << std::setw(2)
        << minutes
        << ':'
        << std::setw(2)
        << seconds;

    return out.str();
}

RuntimeReporter::RuntimeReporter(
    const MiningRuntimeStats& stats,
    std::size_t worker_count,
    std::chrono::milliseconds interval,
    std::ostream& output)
{
    if (worker_count == 0) {
        throw std::invalid_argument(
            "runtime reporter worker count must be positive");
    }

    if (interval <=
        std::chrono::milliseconds{0}) {
        throw std::invalid_argument(
            "runtime reporter interval must be positive");
    }

    m_thread =
        std::jthread(
            [&stats,
             worker_count,
             interval,
             &output](
                std::stop_token token) {
                const auto started =
                    std::chrono::steady_clock::now();

                auto previous_time =
                    started;

                std::uint64_t previous_hashes =
                    stats.completed_hashes.load(
                        std::memory_order_relaxed);

                while (WaitForInterval(
                    token,
                    interval)) {
                    const auto now =
                        std::chrono::steady_clock::now();

                    const auto snapshot =
                        SnapshotMiningRuntimeStats(
                            stats);

                    const double hash_rate =
                        CalculateHashRate(
                            previous_hashes,
                            snapshot.completed_hashes,
                            now - previous_time);

                    std::osyncstream synced{
                        output};

                    synced
                        << std::fixed
                        << std::setprecision(2)
                        << "[status]"
                        << " height="
                        << snapshot.current_height
                        << " workers="
                        << worker_count
                        << " hashrate="
                        << hash_rate
                        << " H/s"
                        << " hashes="
                        << snapshot.completed_hashes
                        << " accepted="
                        << snapshot.accepted_blocks
                        << " stale="
                        << snapshot.stale_work
                        << " rejected="
                        << snapshot.rejected_blocks
                        << " uptime="
                        << FormatMiningUptime(
                            now - started)
                        << '\n';

                    previous_hashes =
                        snapshot.completed_hashes;

                    previous_time = now;
                }
            });
}

void RuntimeReporter::Stop() noexcept
{
    if (!m_thread.joinable()) {
        return;
    }

    m_thread.request_stop();
    m_thread.join();
}

} // namespace mercaminer
