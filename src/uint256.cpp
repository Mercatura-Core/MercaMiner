// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <uint256.h>

#include <algorithm>

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

std::optional<UInt256> UInt256::FromHexBE(std::string_view hex)
{
    if (hex.size() != SIZE * 2) {
        return std::nullopt;
    }

    Storage bytes{};

    for (std::size_t be_index = 0; be_index < SIZE; ++be_index) {
        const int high = HexDigit(hex[be_index * 2]);
        const int low = HexDigit(hex[be_index * 2 + 1]);

        if (high < 0 || low < 0) {
            return std::nullopt;
        }

        bytes[SIZE - 1 - be_index] =
            static_cast<unsigned char>((high << 4) | low);
    }

    return UInt256{bytes};
}

std::string UInt256::ToHexBE() const
{
    static constexpr char HEX[] = "0123456789abcdef";

    std::string result(SIZE * 2, '0');

    for (std::size_t be_index = 0; be_index < SIZE; ++be_index) {
        const unsigned char byte = m_bytes[SIZE - 1 - be_index];
        result[be_index * 2] = HEX[byte >> 4];
        result[be_index * 2 + 1] = HEX[byte & 0x0f];
    }

    return result;
}

bool UInt256::IsZero() const noexcept
{
    return std::all_of(
        m_bytes.begin(),
        m_bytes.end(),
        [](unsigned char byte) { return byte == 0; });
}

int UInt256::CompareNumeric(const UInt256& other) const noexcept
{
    for (std::size_t i = SIZE; i-- > 0;) {
        if (m_bytes[i] < other.m_bytes[i]) return -1;
        if (m_bytes[i] > other.m_bytes[i]) return 1;
    }

    return 0;
}

} // namespace mercaminer
