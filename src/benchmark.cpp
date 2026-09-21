// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <parallel_miner.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>

namespace {

std::size_t ParseWorkers(
    std::string_view text)
{
    std::uint64_t value{};

    const auto result =
        std::from_chars(
            text.data(),
            text.data() + text.size(),
            value);

    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size() ||
        value == 0 ||
        value >
            std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "worker count must be a positive integer");
    }

    return static_cast<std::size_t>(
        value);
}

std::uint64_t ParseSeconds(
    std::string_view text)
{
    std::uint64_t value{};

    const auto result =
        std::from_chars(
            text.data(),
            text.data() + text.size(),
            value);

    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size() ||
        value == 0 ||
        value > 3600) {
        throw std::runtime_error(
            "duration must be between 1 and 3600 seconds");
    }

    return value;
}

mercaminer::UInt256 Parse256(
    const char* hex)
{
    const auto value =
        mercaminer::UInt256::FromHexBE(hex);

    if (!value) {
        throw std::runtime_error(
            "invalid benchmark uint256 constant");
    }

    return *value;
}

mercaminer::BlockCandidate BenchmarkCandidate()
{
    mercaminer::BlockCandidate candidate;

    candidate.header.version = 536870912;

    candidate.header.previous_block =
        Parse256(
            "8e2308efb3a16b126e69444329cc0ed8"
            "1bea0596e99db1032ccd750e7028f685");

    candidate.header.merkle_root =
        Parse256(
            "15fda1757d333bea6c267ba5fc24cd6d"
            "66ba0666e998b894e9d0e95d18125af1");

    candidate.header.time = 1789962799U;
    candidate.header.bits = 0x207fffffU;
    candidate.header.nonce = 0;

    const auto header =
        candidate.header.Serialize();

    candidate.serialized_block.assign(
        header.begin(),
        header.end());

    // Candidate validation requires a block at least as large
    // as its 80-byte header. This byte is not hashed.
    candidate.serialized_block.push_back(0x00);

    return candidate;
}

struct BenchResult
{
    std::uint64_t hashes{};
    double elapsed_seconds{};
};

BenchResult RunInterval(
    mercaminer::ParallelCandidateMiner& miner,
    std::chrono::seconds duration)
{
    auto candidate =
        BenchmarkCandidate();

    // Zero is effectively impossible for a normal 256-bit
    // MercaHash result and prevents intentional early success.
    const mercaminer::UInt256 benchmark_target{};

    std::atomic_bool cancelled{false};

    const auto start =
        std::chrono::steady_clock::now();

    std::thread timer(
        [&] {
            std::this_thread::sleep_for(duration);

            cancelled.store(
                true,
                std::memory_order_relaxed);
        });

    const auto result =
        miner.Mine(
            candidate,
            benchmark_target,
            0,
            std::numeric_limits<std::uint32_t>::max(),
            &cancelled);

    timer.join();

    const auto finish =
        std::chrono::steady_clock::now();

    if (result.status !=
        mercaminer::ScanStatus::CANCELLED) {
        throw std::runtime_error(
            "benchmark ended before timed cancellation");
    }

    const double elapsed =
        std::chrono::duration<double>(
            finish - start).count();

    return BenchResult{
        result.hashes_checked,
        elapsed};
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        if (argc != 3) {
            std::cerr
                << "Usage: "
                << argv[0]
                << " <worker-count> <seconds>\n";

            return 2;
        }

        const std::size_t workers =
            ParseWorkers(argv[1]);

        const std::uint64_t seconds =
            ParseSeconds(argv[2]);

        mercaminer::ParallelCandidateMiner miner{
            workers};

        std::cout
            << "MercaMiner MercaHash benchmark\n"
            << "workers="
            << workers
            << '\n'
            << "scratchpad_mib="
            << (miner.ScratchpadBytes() /
                (1024ULL * 1024ULL))
            << '\n'
            << "warmup_seconds=1\n"
            << "measurement_seconds="
            << seconds
            << '\n';

        // Warm pages, scratchpads and CPU before measurement.
        (void)RunInterval(
            miner,
            std::chrono::seconds{1});

        const BenchResult result =
            RunInterval(
                miner,
                std::chrono::seconds{seconds});

        const double hashes_per_second =
            static_cast<double>(
                result.hashes) /
            result.elapsed_seconds;

        const double per_worker =
            hashes_per_second /
            static_cast<double>(workers);

        std::cout
            << std::fixed
            << std::setprecision(6)
            << "hashes="
            << result.hashes
            << '\n'
            << "elapsed_seconds="
            << result.elapsed_seconds
            << '\n'
            << "hashes_per_second="
            << hashes_per_second
            << '\n'
            << "hashes_per_worker_second="
            << per_worker
            << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "MercaMiner benchmark failed: "
            << error.what()
            << '\n';

        return 1;
    }
}
