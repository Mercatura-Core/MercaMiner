// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <scanner.h>

#include <crypto/mercahash.h>

#include <atomic>
#include <cstddef>
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

mercaminer::BlockHeader IncrementalHeader()
{
    mercaminer::BlockHeader header;

    // This reproduces the permanent MercaHash test header
    // containing bytes 00,01,...,4f.
    header.version =
        static_cast<std::int32_t>(0x03020100U);

    mercaminer::UInt256::Storage previous{};
    for (std::size_t i = 0; i < previous.size(); ++i) {
        previous[i] =
            static_cast<unsigned char>(4 + i);
    }
    header.previous_block =
        mercaminer::UInt256{previous};

    mercaminer::UInt256::Storage merkle{};
    for (std::size_t i = 0; i < merkle.size(); ++i) {
        merkle[i] =
            static_cast<unsigned char>(36 + i);
    }
    header.merkle_root =
        mercaminer::UInt256{merkle};

    header.time = 0x47464544U;
    header.bits = 0x4b4a4948U;
    header.nonce = 0x4f4e4d4cU;

    return header;
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
    using mercaminer::NonceScanner;
    using mercaminer::ScanStatus;

    bool ok{true};

    NonceScanner scanner;

    ok &= Check(
        scanner.ScratchpadSize() ==
            mercahash::SCRATCHPAD_BYTES,
        "scanner owns one 128 MiB scratchpad");

    const auto header =
        IncrementalHeader();

    const std::uint32_t known_nonce =
        0x4f4e4d4cU;

    // Permanent MercaHash raw output:
    // 232171...f7d8
    //
    // UInt256 display form reverses those raw internal bytes.
    const auto exact_target =
        Parse256(
            "d8f7179f4c633c1ef158399e6181227e"
            "e1791df2c4c08689870215f22a712123");

    const auto exact =
        scanner.Scan(
            header,
            exact_target,
            known_nonce,
            known_nonce);

    ok &= Check(
        exact.status == ScanStatus::FOUND &&
            exact.nonce == known_nonce &&
            exact.hash == exact_target &&
            exact.hashes_checked == 1,
        "known Core MercaHash vector meets exact target");

    const auto below_exact =
        Parse256(
            "d8f7179f4c633c1ef158399e6181227e"
            "e1791df2c4c08689870215f22a712122");

    const auto exhausted =
        scanner.Scan(
            header,
            below_exact,
            known_nonce,
            known_nonce);

    ok &= Check(
        exhausted.status == ScanStatus::EXHAUSTED &&
            exhausted.nonce == known_nonce &&
            exhausted.hash == exact_target &&
            exhausted.hashes_checked == 1,
        "single-nonce range exhausts below target");

    const auto maximum_target =
        Parse256(
            "ffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffff");

    mercaminer::BlockHeader maximum_nonce_header{};
    maximum_nonce_header.nonce =
        0xffffffffU;

    const auto maximum_nonce =
        scanner.Scan(
            maximum_nonce_header,
            maximum_target,
            0xffffffffU,
            0xffffffffU);

    ok &= Check(
        maximum_nonce.status == ScanStatus::FOUND &&
            maximum_nonce.nonce == 0xffffffffU &&
            maximum_nonce.hashes_checked == 1,
        "uint32 maximum nonce is scanned inclusively");

    std::atomic_bool cancelled{true};

    const auto cancelled_result =
        scanner.Scan(
            header,
            maximum_target,
            100,
            200,
            &cancelled);

    ok &= Check(
        cancelled_result.status ==
            ScanStatus::CANCELLED &&
            cancelled_result.hashes_checked == 0,
        "pre-set cancellation stops before hashing");

    ok &= Check(
        ThrowsInvalidArgument([&] {
            scanner.Scan(
                header,
                maximum_target,
                2,
                1);
        }),
        "reversed nonce range rejected");

    mercaminer::BlockHeader regtest_candidate;

    regtest_candidate.version = 536870912;

    regtest_candidate.previous_block =
        Parse256(
            "8e2308efb3a16b126e69444329cc0ed8"
            "1bea0596e99db1032ccd750e7028f685");

    regtest_candidate.merkle_root =
        Parse256(
            "15fda1757d333bea6c267ba5fc24cd6d"
            "66ba0666e998b894e9d0e95d18125af1");

    regtest_candidate.time = 1789962799U;
    regtest_candidate.bits = 0x207fffffU;
    regtest_candidate.nonce = 0;

    const auto regtest_target =
        Parse256(
            "7fffff00000000000000000000000000"
            "00000000000000000000000000000000");

    const auto regtest_scan =
        scanner.Scan(
            regtest_candidate,
            regtest_target,
            0,
            31);

    const auto expected_regtest_hash =
        Parse256(
            "7e3455749db5d93d98dc663972fbb9f3"
            "ec7223c95cd9a92006d418a8537c5bd3");

    ok &= Check(
        regtest_scan.status == ScanStatus::FOUND &&
            regtest_scan.nonce == 0 &&
            regtest_scan.hash == expected_regtest_hash &&
            regtest_scan.hashes_checked == 1,
        "Core-cross-checked regtest PoW vector");

    return ok ? 0 : 1;
}
