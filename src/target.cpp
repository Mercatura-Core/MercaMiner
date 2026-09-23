// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <target.h>

#include <cstddef>
#include <cstdint>

namespace mercaminer {
namespace {

int HexDigit(char c) noexcept
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

CompactTarget DecodeCompact(std::uint32_t compact) noexcept
{
    const int size = static_cast<int>(compact >> 24);
    std::uint32_t word = compact & 0x007fffffU;

    UInt256::Storage bytes{};

    if (size <= 3) {
        word >>= 8 * (3 - size);

        for (std::size_t i = 0; i < 4 && i < bytes.size(); ++i) {
            bytes[i] =
                static_cast<unsigned char>(word >> (8 * i));
        }
    } else {
        const std::size_t offset =
            static_cast<std::size_t>(size - 3);

        for (std::size_t i = 0; i < 3; ++i) {
            if (offset + i < bytes.size()) {
                bytes[offset + i] =
                    static_cast<unsigned char>(word >> (8 * i));
            }
        }
    }

    const bool negative =
        word != 0 &&
        (compact & 0x00800000U) != 0;

    const bool overflow =
        word != 0 &&
        ((size > 34) ||
         (word > 0xffU && size > 33) ||
         (word > 0xffffU && size > 32));

    return CompactTarget{
        UInt256{bytes},
        negative,
        overflow,
    };
}

std::optional<UInt256>
ValidTargetFromCompact(std::uint32_t compact) noexcept
{
    const CompactTarget decoded = DecodeCompact(compact);

    if (!decoded.IsValid()) {
        return std::nullopt;
    }

    return decoded.target;
}

std::optional<std::uint32_t>
ParseBitsHex(std::string_view hex) noexcept
{
    if (hex.size() != 8) {
        return std::nullopt;
    }

    std::uint32_t value{0};

    for (const char c : hex) {
        const int digit = HexDigit(c);

        if (digit < 0) {
            return std::nullopt;
        }

        value =
            (value << 4) |
            static_cast<std::uint32_t>(digit);
    }

    return value;
}

bool MeetsTarget(
    const UInt256& hash,
    const UInt256& target) noexcept
{
    return hash <= target;
}

} // namespace mercaminer
