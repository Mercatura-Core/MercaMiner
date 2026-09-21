// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <hash256.h>
#include <merkle.h>
#include <serialization.h>
#include <uint256.h>

#include <array>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string Hex(
    std::span<const unsigned char> bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (const unsigned char byte : bytes) {
        out << std::setw(2)
            << static_cast<unsigned int>(byte);
    }

    return out.str();
}

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

} // namespace

int main()
{
    using mercaminer::AppendCompactSize;
    using mercaminer::AppendLE16;
    using mercaminer::AppendLE32;
    using mercaminer::AppendLE64;
    using mercaminer::Bytes;
    using mercaminer::ComputeMerkleRoot;
    using mercaminer::DoubleSha256;
    using mercaminer::Sha256;

    bool ok{true};

    Bytes little_endian;

    AppendLE16(
        little_endian,
        0x1234U);

    AppendLE32(
        little_endian,
        0x89abcdefU);

    AppendLE64(
        little_endian,
        0x0123456789abcdefULL);

    ok &= Check(
        Hex(little_endian) ==
            "3412"
            "efcdab89"
            "efcdab8967452301",
        "little-endian integer serialization");

    struct CompactVector
    {
        std::uint64_t value;
        const char* expected;
    };

    static constexpr CompactVector compact_vectors[]{
        {0, "00"},
        {252, "fc"},
        {253, "fdfd00"},
        {65535, "fdffff"},
        {65536, "fe00000100"},
        {0xffffffffULL, "feffffffff"},
        {0x100000000ULL, "ff0000000001000000"},
    };

    for (const auto& vector : compact_vectors) {
        Bytes encoded;
        AppendCompactSize(
            encoded,
            vector.value);

        ok &= Check(
            Hex(encoded) ==
                vector.expected,
            "CompactSize boundary vector");
    }

    const std::array<unsigned char, 0>
        empty{};

    const auto sha_empty =
        Sha256(empty);

    ok &= Check(
        Hex(sha_empty) ==
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
        "SHA256 empty vector");

    static constexpr std::array<unsigned char, 3>
        abc{{'a', 'b', 'c'}};

    const auto sha_abc =
        Sha256(abc);

    ok &= Check(
        Hex(sha_abc) ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
        "SHA256 abc vector");

    const auto double_empty =
        DoubleSha256(empty);

    ok &= Check(
        Hex(double_empty.bytes()) ==
            "5df6e0e2761359d30a8275058e299fcc"
            "0381534545f55cf43e41983f5d4c9456",
        "double SHA256 empty raw bytes");

    ok &= Check(
        double_empty.ToHexBE() ==
            "56944c5d3f98413ef45cf54545538103"
            "cc9f298e0575820ad3591376e2e0f65d",
        "double SHA256 uint256 display semantics");

    const auto empty_merkle =
        ComputeMerkleRoot({});

    ok &= Check(
        empty_merkle.root.IsZero() &&
            !empty_merkle.mutated,
        "empty merkle tree returns zero");

    const auto leaf1 =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000001");

    const auto leaf2 =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000002");

    const auto leaf3 =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000003");

    const auto single =
        ComputeMerkleRoot({leaf1});

    ok &= Check(
        single.root == leaf1 &&
            !single.mutated,
        "single-leaf merkle root unchanged");

    const auto three =
        ComputeMerkleRoot(
            {leaf1, leaf2, leaf3});

    ok &= Check(
        three.root.ToHexBE() ==
            "e9ffb584c62449f157c8be88257bd1ee"
            "bb2d8ef824f5c86b43c4f8fd9e800d6a" &&
            !three.mutated,
        "three-leaf merkle root vector");

    const auto odd_duplicate =
        ComputeMerkleRoot(
            {leaf1, leaf2, leaf3});

    ok &= Check(
        !odd_duplicate.mutated,
        "implicit odd-leaf duplication is not mutation");

    const auto explicit_duplicate =
        ComputeMerkleRoot(
            {leaf1, leaf1});

    ok &= Check(
        explicit_duplicate.mutated,
        "explicit duplicate pair marks mutation");

    return ok ? 0 : 1;
}
