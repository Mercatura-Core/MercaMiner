// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <parallel_miner.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

bool Check(
    bool condition,
    const char* name)
{
    if (!condition) {
        std::cerr
            << "FAIL "
            << name
            << '\n';

        return false;
    }

    std::cout
        << "PASS "
        << name
        << '\n';

    return true;
}

mercaminer::UInt256 Parse256(
    const char* hex)
{
    const auto value =
        mercaminer::UInt256::FromHexBE(hex);

    if (!value) {
        throw std::runtime_error(
            "invalid test uint256 constant");
    }

    return *value;
}

mercaminer::BlockCandidate
CoreCheckedCandidate()
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

    const auto serialized =
        candidate.header.Serialize();

    candidate.serialized_block.assign(
        serialized.begin(),
        serialized.end());

    candidate.serialized_block.push_back(0x00);

    return candidate;
}

template <typename Callable>
bool ThrowsInvalidArgument(
    Callable&& callable)
{
    try {
        callable();
    } catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    using mercaminer::ParallelCandidateMiner;
    using mercaminer::PartitionNonceRange;
    using mercaminer::ScanStatus;

    bool ok{true};

    const auto full =
        PartitionNonceRange(
            0,
            std::numeric_limits<std::uint32_t>::max(),
            3);

    ok &= Check(
        full.size() == 3 &&
            full[0].begin == 0x00000000U &&
            full[0].end == 0x55555555U &&
            full[1].begin == 0x55555556U &&
            full[1].end == 0xAAAAAAAAU &&
            full[2].begin == 0xAAAAAAABU &&
            full[2].end == 0xFFFFFFFFU,
        "full uint32 nonce space partitioned without overlap");

    const auto small =
        PartitionNonceRange(
            10,
            11,
            4);

    ok &= Check(
        small.size() == 2 &&
            small[0].begin == 10 &&
            small[0].end == 10 &&
            small[1].begin == 11 &&
            small[1].end == 11,
        "workers capped by available nonce count");

    ok &= Check(
        ThrowsInvalidArgument([] {
            (void)PartitionNonceRange(
                0,
                1,
                0);
        }),
        "zero partition worker count rejected");

    ok &= Check(
        ThrowsInvalidArgument([] {
            (void)PartitionNonceRange(
                2,
                1,
                1);
        }),
        "reversed partition range rejected");

    ok &= Check(
        ThrowsInvalidArgument([] {
            ParallelCandidateMiner miner{0};
        }),
        "zero parallel worker count rejected");

    ParallelCandidateMiner miner{2};

    ok &= Check(
        miner.WorkerCount() == 2,
        "parallel miner preserves worker count");

    ok &= Check(
        miner.ScratchpadBytes() ==
            2ULL * 128ULL * 1024ULL * 1024ULL,
        "one 128 MiB scratchpad allocated per worker");

    const auto maximum_target =
        Parse256(
            "ffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffff");

    auto candidate =
        CoreCheckedCandidate();

    const auto found =
        miner.Mine(
            candidate,
            maximum_target,
            0,
            1);

    ok &= Check(
        found.status == ScanStatus::FOUND &&
            found.nonce <= 1 &&
            found.hashes_checked >= 1 &&
            found.hashes_checked <= 2 &&
            found.winning_worker.has_value() &&
            found.active_workers == 2,
        "parallel miner finds candidate");

    ok &= Check(
        candidate.header.nonce ==
            found.nonce &&
            candidate.serialized_block[76] ==
                static_cast<unsigned char>(
                    found.nonce & 0xff),
        "parallel winner applied to shared candidate once");

    const auto impossible_target =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000001");

    auto exhausted_candidate =
        CoreCheckedCandidate();

    const auto exhausted_original =
        exhausted_candidate.serialized_block;

    const auto exhausted =
        miner.Mine(
            exhausted_candidate,
            impossible_target,
            0,
            1);

    ok &= Check(
        exhausted.status ==
                ScanStatus::EXHAUSTED &&
            exhausted.hashes_checked == 2 &&
            exhausted.active_workers == 2,
        "parallel nonce ranges exhaust exactly once");

    ok &= Check(
        exhausted_candidate.serialized_block ==
            exhausted_original,
        "parallel exhaustion leaves candidate unchanged");

    auto cancelled_candidate =
        CoreCheckedCandidate();

    const auto cancelled_original =
        cancelled_candidate.serialized_block;

    std::atomic_bool cancelled{true};

    const auto cancelled_result =
        miner.Mine(
            cancelled_candidate,
            maximum_target,
            0,
            31,
            &cancelled);

    ok &= Check(
        cancelled_result.status ==
                ScanStatus::CANCELLED &&
            cancelled_result.hashes_checked == 0,
        "external pre-cancellation stops all parallel workers");

    ok &= Check(
        cancelled_candidate.serialized_block ==
            cancelled_original,
        "parallel cancellation leaves candidate unchanged");

    return ok ? 0 : 1;
}
