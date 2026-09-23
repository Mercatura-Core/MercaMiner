// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_TARGET_H
#define MERCAMINER_TARGET_H

#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace mercaminer {

struct CompactTarget
{
    UInt256 target{};
    bool negative{false};
    bool overflow{false};

    bool IsValid() const noexcept
    {
        return !negative && !overflow && !target.IsZero();
    }
};

CompactTarget DecodeCompact(std::uint32_t compact) noexcept;

std::optional<UInt256>
ValidTargetFromCompact(std::uint32_t compact) noexcept;

std::optional<std::uint32_t>
ParseBitsHex(std::string_view hex) noexcept;

bool MeetsTarget(
    const UInt256& hash,
    const UInt256& target) noexcept;

} // namespace mercaminer

#endif // MERCAMINER_TARGET_H
