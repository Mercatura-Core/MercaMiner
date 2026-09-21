// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <serialization.h>

namespace mercaminer {

void AppendBytes(
    Bytes& out,
    std::span<const unsigned char> bytes)
{
    out.insert(
        out.end(),
        bytes.begin(),
        bytes.end());
}

void AppendLE16(
    Bytes& out,
    std::uint16_t value)
{
    out.push_back(
        static_cast<unsigned char>(value));
    out.push_back(
        static_cast<unsigned char>(value >> 8));
}

void AppendLE32(
    Bytes& out,
    std::uint32_t value)
{
    out.push_back(
        static_cast<unsigned char>(value));
    out.push_back(
        static_cast<unsigned char>(value >> 8));
    out.push_back(
        static_cast<unsigned char>(value >> 16));
    out.push_back(
        static_cast<unsigned char>(value >> 24));
}

void AppendLE64(
    Bytes& out,
    std::uint64_t value)
{
    for (unsigned int shift = 0;
         shift < 64;
         shift += 8) {
        out.push_back(
            static_cast<unsigned char>(
                value >> shift));
    }
}

void AppendCompactSize(
    Bytes& out,
    std::uint64_t value)
{
    if (value < 253) {
        out.push_back(
            static_cast<unsigned char>(value));
        return;
    }

    if (value <= 0xffffU) {
        out.push_back(253);
        AppendLE16(
            out,
            static_cast<std::uint16_t>(value));
        return;
    }

    if (value <= 0xffffffffULL) {
        out.push_back(254);
        AppendLE32(
            out,
            static_cast<std::uint32_t>(value));
        return;
    }

    out.push_back(255);
    AppendLE64(out, value);
}

} // namespace mercaminer
