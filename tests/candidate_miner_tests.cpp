// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <candidate_miner.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
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

    const auto serialized_header =
        candidate.header.Serialize();

    candidate.serialized_block.assign(
        serialized_header.begin(),
        serialized_header.end());

    // One dummy byte is enough here because this test is about
    // preserving/updating the 80-byte header inside a candidate.
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
    using mercaminer::MineBlockCandidate;
    using mercaminer::NonceScanner;
    using mercaminer::ScanStatus;

    bool ok{true};

    NonceScanner scanner;

    const auto regtest_target =
        Parse256(
            "7fffff00000000000000000000000000"
            "00000000000000000000000000000000");

    auto candidate =
        CoreCheckedCandidate();

    const auto maximum_target =
        Parse256(
            "ffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffff");

    const auto result =
        MineBlockCandidate(
            scanner,
            candidate,
            maximum_target,
            1,
            1);

    ok &= Check(
        result.status == ScanStatus::FOUND &&
            result.nonce == 1 &&
            result.hashes_checked == 1,
        "candidate nonce one found");

    ok &= Check(
        candidate.header.nonce == 1,
        "nonzero nonce applied to candidate header");

    ok &= Check(
        candidate.serialized_block[76] == 0x01 &&
            candidate.serialized_block[77] == 0x00 &&
            candidate.serialized_block[78] == 0x00 &&
            candidate.serialized_block[79] == 0x00,
        "nonzero nonce applied to serialized block");

    const auto solved_header =
        candidate.header.Serialize();

    ok &= Check(
        std::equal(
            solved_header.begin(),
            solved_header.end(),
            candidate.serialized_block.begin()),
        "serialized block header matches solved header");

    auto exhausted_candidate =
        CoreCheckedCandidate();

    const auto original_header =
        exhausted_candidate.header;

    const auto original_block =
        exhausted_candidate.serialized_block;

    const auto impossible_target =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000001");

    const auto exhausted =
        MineBlockCandidate(
            scanner,
            exhausted_candidate,
            impossible_target,
            0,
            0);

    ok &= Check(
        exhausted.status == ScanStatus::EXHAUSTED &&
            exhausted.hashes_checked == 1,
        "exhausted candidate scan reported");

    ok &= Check(
        exhausted_candidate.header.nonce ==
                original_header.nonce &&
            exhausted_candidate.serialized_block ==
                original_block,
        "exhausted scan leaves candidate unchanged");

    auto cancelled_candidate =
        CoreCheckedCandidate();

    const auto cancelled_header =
        cancelled_candidate.header;

    const auto cancelled_block =
        cancelled_candidate.serialized_block;

    std::atomic_bool cancelled{true};

    const auto cancelled_result =
        MineBlockCandidate(
            scanner,
            cancelled_candidate,
            regtest_target,
            0,
            31,
            &cancelled);

    ok &= Check(
        cancelled_result.status ==
                ScanStatus::CANCELLED &&
            cancelled_result.hashes_checked == 0,
        "cancelled candidate scan reported");

    ok &= Check(
        cancelled_candidate.header.nonce ==
                cancelled_header.nonce &&
            cancelled_candidate.serialized_block ==
                cancelled_block,
        "cancelled scan leaves candidate unchanged");

    auto inconsistent =
        CoreCheckedCandidate();

    inconsistent.header.nonce = 1;

    ok &= Check(
        ThrowsInvalidArgument([&] {
            MineBlockCandidate(
                scanner,
                inconsistent,
                regtest_target,
                0,
                0);
        }),
        "header and serialized-block mismatch rejected");

    mercaminer::BlockCandidate malformed;
    malformed.serialized_block.resize(79);

    ok &= Check(
        ThrowsInvalidArgument([&] {
            MineBlockCandidate(
                scanner,
                malformed,
                regtest_target,
                0,
                0);
        }),
        "short serialized block rejected");

    return ok ? 0 : 1;
}
