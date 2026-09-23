// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_UINT256_H
#define MERCAMINER_UINT256_H

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace mercaminer {

class UInt256
{
public:
    static constexpr std::size_t SIZE = 32;
    using Storage = std::array<unsigned char, SIZE>;

    constexpr UInt256() = default;
    explicit constexpr UInt256(Storage bytes) : m_bytes(bytes) {}

    static std::optional<UInt256> FromHexBE(std::string_view hex);

    std::string ToHexBE() const;
    bool IsZero() const noexcept;

    const Storage& bytes() const noexcept { return m_bytes; }

    int CompareNumeric(const UInt256& other) const noexcept;

    friend bool operator==(const UInt256&, const UInt256&) = default;

    friend bool operator<(const UInt256& a, const UInt256& b) noexcept
    {
        return a.CompareNumeric(b) < 0;
    }

    friend bool operator<=(const UInt256& a, const UInt256& b) noexcept
    {
        return a.CompareNumeric(b) <= 0;
    }

    friend bool operator>(const UInt256& a, const UInt256& b) noexcept
    {
        return a.CompareNumeric(b) > 0;
    }

    friend bool operator>=(const UInt256& a, const UInt256& b) noexcept
    {
        return a.CompareNumeric(b) >= 0;
    }

private:
    Storage m_bytes{};
};

} // namespace mercaminer

#endif // MERCAMINER_UINT256_H
