// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_HEADER_H
#define MERCAMINER_HEADER_H

#include <uint256.h>

#include <array>
#include <cstdint>

namespace mercaminer {

struct BlockHeader
{
    static constexpr std::size_t SERIALIZED_SIZE = 80;

    std::int32_t version{};
    UInt256 previous_block{};
    UInt256 merkle_root{};
    std::uint32_t time{};
    std::uint32_t bits{};
    std::uint32_t nonce{};

    std::array<unsigned char, SERIALIZED_SIZE> Serialize() const noexcept;
};

} // namespace mercaminer

#endif // MERCAMINER_HEADER_H
