// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <crypto/mercahash.h>
#include <header.h>
#include <target.h>
#include <uint256.h>

#include <array>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string Hex(std::span<const unsigned char> bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (const unsigned char byte : bytes) {
        out << std::setw(2)
            << static_cast<unsigned int>(byte);
    }

    return out.str();
}

bool Check(bool condition, const char* name)
{
    if (!condition) {
        std::cerr << "FAIL " << name << '\n';
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

} // namespace

int main()
{
    using mercaminer::BlockHeader;
    using mercaminer::UInt256;

    bool ok{true};

    const auto previous_block = UInt256::FromHexBE(
        "232221201f1e1d1c1b1a191817161514"
        "131211100f0e0d0c0b0a090807060504");

    const auto merkle_root = UInt256::FromHexBE(
        "434241403f3e3d3c3b3a393837363534"
        "333231302f2e2d2c2b2a292827262524");

    ok &= Check(
        previous_block.has_value(),
        "previous-block display hex parses");

    ok &= Check(
        merkle_root.has_value(),
        "merkle-root display hex parses");

    if (!previous_block || !merkle_root) {
        return 1;
    }

    BlockHeader header{
        .version = static_cast<std::int32_t>(0x03020100U),
        .previous_block = *previous_block,
        .merkle_root = *merkle_root,
        .time = 0x47464544U,
        .bits = 0x4b4a4948U,
        .nonce = 0x4f4e4d4cU,
    };

    const auto serialized = header.Serialize();

    std::array<unsigned char, BlockHeader::SERIALIZED_SIZE> expected{};

    for (std::size_t i = 0; i < expected.size(); ++i) {
        expected[i] = static_cast<unsigned char>(i);
    }

    ok &= Check(
        serialized == expected,
        "canonical CBlockHeader serialization is 00..4f");

    std::vector<unsigned char> scratchpad(
        mercahash::SCRATCHPAD_BYTES);

    std::array<unsigned char, mercahash::OUTPUT_SIZE> output{};

    mercahash::HashV1(
        std::span<const unsigned char>{serialized},
        std::span<unsigned char>{scratchpad},
        std::span<unsigned char>{output});

    ok &= Check(
        Hex(output) ==
            "2321712af21502878986c0c4f21d79e1"
            "7e2281619e3958f11e3c634c9f17f7d8",
        "serialized header matches Core MercaHash bridge vector");

    const UInt256 pow_hash{output};

    ok &= Check(
        pow_hash.bytes() == output,
        "MercaHash bytes map directly to uint256 storage");

    static constexpr const char* POW_HASH_DISPLAY =
        "d8f7179f4c633c1ef158399e6181227e"
        "e1791df2c4c08689870215f22a712123";

    ok &= Check(
        pow_hash.ToHexBE() == POW_HASH_DISPLAY,
        "MercaHash uint256 display reverses raw bytes like Core");

    const auto exact_target =
        UInt256::FromHexBE(POW_HASH_DISPLAY);

    ok &= Check(
        exact_target.has_value() &&
            mercaminer::MeetsTarget(pow_hash, *exact_target),
        "raw MercaHash numeric value equals parsed target");

    return ok ? 0 : 1;
}
