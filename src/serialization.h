// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_SERIALIZATION_H
#define MERCAMINER_SERIALIZATION_H

#include <cstdint>
#include <span>
#include <vector>

namespace mercaminer {

using Bytes = std::vector<unsigned char>;

void AppendBytes(
    Bytes& out,
    std::span<const unsigned char> bytes);

void AppendLE16(
    Bytes& out,
    std::uint16_t value);

void AppendLE32(
    Bytes& out,
    std::uint32_t value);

void AppendLE64(
    Bytes& out,
    std::uint64_t value);

void AppendCompactSize(
    Bytes& out,
    std::uint64_t value);

} // namespace mercaminer

#endif // MERCAMINER_SERIALIZATION_H
