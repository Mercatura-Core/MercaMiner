// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <header.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace mercaminer {
namespace {

void WriteLE32(unsigned char* out, std::uint32_t value) noexcept
{
    out[0] = static_cast<unsigned char>(value);
    out[1] = static_cast<unsigned char>(value >> 8);
    out[2] = static_cast<unsigned char>(value >> 16);
    out[3] = static_cast<unsigned char>(value >> 24);
}

} // namespace

std::array<unsigned char, BlockHeader::SERIALIZED_SIZE>
BlockHeader::Serialize() const noexcept
{
    std::array<unsigned char, SERIALIZED_SIZE> out{};

    WriteLE32(
        out.data(),
        static_cast<std::uint32_t>(version));

    std::copy(
        previous_block.bytes().begin(),
        previous_block.bytes().end(),
        out.begin() + 4);

    std::copy(
        merkle_root.bytes().begin(),
        merkle_root.bytes().end(),
        out.begin() + 36);

    WriteLE32(out.data() + 68, time);
    WriteLE32(out.data() + 72, bits);
    WriteLE32(out.data() + 76, nonce);

    return out;
}

} // namespace mercaminer
