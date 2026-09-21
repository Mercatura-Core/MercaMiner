// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <target.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

bool Check(bool condition, const char* name)
{
    if (!condition) {
        std::cerr << "FAIL " << name << '\n';
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

mercaminer::UInt256 LowerByOne(
    const mercaminer::UInt256& value)
{
    auto bytes = value.bytes();

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] != 0) {
            --bytes[i];
            break;
        }

        bytes[i] = 0xff;
    }

    return mercaminer::UInt256{bytes};
}

mercaminer::UInt256 HigherByOne(
    const mercaminer::UInt256& value)
{
    auto bytes = value.bytes();

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        ++bytes[i];

        if (bytes[i] != 0) {
            break;
        }
    }

    return mercaminer::UInt256{bytes};
}

} // namespace

int main()
{
    using mercaminer::DecodeCompact;
    using mercaminer::MeetsTarget;
    using mercaminer::ParseBitsHex;
    using mercaminer::UInt256;
    using mercaminer::ValidTargetFromCompact;

    bool ok{true};

    static constexpr const char* DIFF1_TARGET =
        "00000000ffff00000000000000000000"
        "00000000000000000000000000000000";

    const auto parsed_target =
        UInt256::FromHexBE(DIFF1_TARGET);

    ok &= Check(
        parsed_target.has_value(),
        "64-digit display target parses");

    if (!parsed_target) {
        return 1;
    }

    ok &= Check(
        parsed_target->ToHexBE() == DIFF1_TARGET,
        "display target round-trips");

    ok &= Check(
        !UInt256::FromHexBE("00").has_value(),
        "short uint256 hex rejected");

    ok &= Check(
        !UInt256::FromHexBE(
            "00000000ffff00000000000000000000"
            "0000000000000000000000000000000g").has_value(),
        "non-hex uint256 rejected");

    const auto bits = ParseBitsHex("1d00ffff");

    ok &= Check(
        bits.has_value() &&
            *bits == 0x1d00ffffU,
        "GBT bits parses");

    ok &= Check(
        !ParseBitsHex("1d00fff").has_value(),
        "short GBT bits rejected");

    ok &= Check(
        !ParseBitsHex("1d00fffg").has_value(),
        "non-hex GBT bits rejected");

    const auto diff1 =
        ValidTargetFromCompact(0x1d00ffffU);

    ok &= Check(
        diff1.has_value(),
        "0x1d00ffff compact target valid");

    if (!diff1) {
        return 1;
    }

    ok &= Check(
        diff1->ToHexBE() == DIFF1_TARGET,
        "0x1d00ffff decodes exactly");

    ok &= Check(
        *diff1 == *parsed_target,
        "GBT full target agrees with nBits target");

    ok &= Check(
        !ValidTargetFromCompact(0x00000000U).has_value(),
        "zero compact target rejected");

    ok &= Check(
        !ValidTargetFromCompact(0x00123456U).has_value(),
        "Core zero-size compact target rejected");

    ok &= Check(
        !ValidTargetFromCompact(0x01003456U).has_value(),
        "Core shifted-to-zero compact target rejected");

    const auto shifted_negative =
        DecodeCompact(0x01803456U);

    ok &= Check(
        shifted_negative.target.IsZero() &&
            !shifted_negative.negative &&
            !shifted_negative.overflow,
        "Core shifted-to-zero sign semantics preserved");

    const auto negative =
        DecodeCompact(0x04923456U);

    ok &= Check(
        negative.negative &&
            !negative.overflow &&
            !negative.target.IsZero(),
        "negative compact target detected");

    ok &= Check(
        !ValidTargetFromCompact(0x04923456U).has_value(),
        "negative compact target rejected");

    const auto huge =
        DecodeCompact(0xff123456U);

    ok &= Check(
        huge.overflow,
        "large compact target overflow detected");

    ok &= Check(
        !ValidTargetFromCompact(0xff123456U).has_value(),
        "overflow compact target rejected");

    const auto size32 =
        ValidTargetFromCompact(0x20123456U);

    ok &= Check(
        size32.has_value() &&
            size32->ToHexBE() ==
                std::string("123456") +
                std::string(58, '0'),
        "32-byte compact target boundary valid");

    const auto size33 =
        ValidTargetFromCompact(0x2100ffffU);

    ok &= Check(
        size33.has_value() &&
            size33->ToHexBE() ==
                std::string("ffff") +
                std::string(60, '0'),
        "33-byte compact target boundary valid");

    ok &= Check(
        !ValidTargetFromCompact(0x21010000U).has_value(),
        "33-byte overflow boundary rejected");

    const auto size34 =
        ValidTargetFromCompact(0x220000ffU);

    ok &= Check(
        size34.has_value() &&
            size34->ToHexBE() ==
                std::string("ff") +
                std::string(62, '0'),
        "34-byte compact target boundary valid");

    ok &= Check(
        !ValidTargetFromCompact(0x22000100U).has_value(),
        "34-byte overflow boundary rejected");

    const UInt256 lower = LowerByOne(*diff1);
    const UInt256 equal = *diff1;
    const UInt256 higher = HigherByOne(*diff1);

    ok &= Check(
        MeetsTarget(lower, *diff1),
        "hash below target accepted");

    ok &= Check(
        MeetsTarget(equal, *diff1),
        "hash equal to target accepted");

    ok &= Check(
        !MeetsTarget(higher, *diff1),
        "hash above target rejected");

    return ok ? 0 : 1;
}
