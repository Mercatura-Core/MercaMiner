// Copyright (c) 2026 The Mercatura Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <crypto/mercahash.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Header = std::array<unsigned char, mercahash::HEADER_SIZE>;
using Hash = std::array<unsigned char, mercahash::OUTPUT_SIZE>;

std::string Hex(std::span<const unsigned char> bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (const unsigned char byte : bytes) {
        out << std::setw(2) << static_cast<unsigned int>(byte);
    }

    return out.str();
}

bool CheckVector(
    std::string_view name,
    const Header& header,
    std::string_view expected_hex,
    std::span<unsigned char> scratchpad)
{
    Hash actual{};

    mercahash::HashV1(
        header,
        scratchpad,
        actual);

    const std::string actual_hex{Hex(actual)};

    if (actual_hex != expected_hex) {
        std::cerr
            << "FAIL " << name << '\n'
            << "  expected: " << expected_hex << '\n'
            << "  actual:   " << actual_hex << '\n';
        return false;
    }

    std::cout << "PASS " << name << ": " << actual_hex << '\n';
    return true;
}

} // namespace

int main()
{
    static_assert(mercahash::HEADER_SIZE == 80);
    static_assert(mercahash::OUTPUT_SIZE == 32);
    static_assert(
        mercahash::SCRATCHPAD_BYTES ==
        128ULL * 1024 * 1024);
    static_assert(mercahash::LINE_BYTES == 64);
    static_assert(mercahash::LINE_COUNT == 2'097'152);
    static_assert(mercahash::BIND_BLOCK_BYTES == 4096);
    static_assert(mercahash::BIND_BLOCK_LINES == 64);
    static_assert(mercahash::BIND_BLOCK_COUNT == 32'768);
    static_assert(mercahash::SECONDARY_INTERVAL == 16);
    static_assert(mercahash::CROSS_LANE_INTERVAL == 64);
    static_assert(mercahash::CHECKPOINT_INTERVAL == 1024);
    static_assert(mercahash::FINAL_READS == 256);
    static_assert(mercahash::V1_MIX_STEPS == 131'072);
    static_assert(mercahash::V1_BINDING_PASSES == 1);

    std::vector<unsigned char> scratchpad(
        mercahash::SCRATCHPAD_BYTES);

    Header zero{};

    Header incremental{};
    for (std::size_t i = 0; i < incremental.size(); ++i) {
        incremental[i] = static_cast<unsigned char>(i);
    }

    Header all_ff{};
    all_ff.fill(0xff);

    Header mixed{};
    for (std::size_t i = 0; i < mixed.size(); ++i) {
        mixed[i] =
            static_cast<unsigned char>((i * 37 + 11) & 0xff);
    }

    bool ok{true};

    ok &= CheckVector(
        "zero",
        zero,
        "cc97e7cbce1d4ed2b54c9c37a81a0003"
        "83229341a277a44829484855b9ef79dc",
        scratchpad);

    ok &= CheckVector(
        "incremental",
        incremental,
        "2321712af21502878986c0c4f21d79e1"
        "7e2281619e3958f11e3c634c9f17f7d8",
        scratchpad);

    ok &= CheckVector(
        "all-ff",
        all_ff,
        "9c9a427928875304bb7d6351354b5e50"
        "ec8e57c581971f68eab176371c4cd2c2",
        scratchpad);

    ok &= CheckVector(
        "mixed",
        mixed,
        "1244c6c9e0365baeb5958cfaa57aff58"
        "2d81ae1d757769e823085bb00676e170",
        scratchpad);

    if (!ok) {
        return 1;
    }

    std::cout
        << "All permanent Mercatura Core MercaHash V1 vectors passed.\n";

    return 0;
}
