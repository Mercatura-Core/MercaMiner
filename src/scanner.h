// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_SCANNER_H
#define MERCAMINER_SCANNER_H

#include <header.h>
#include <uint256.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mercaminer {

enum class ScanStatus
{
    FOUND,
    EXHAUSTED,
    CANCELLED,
};

struct ScanResult
{
    ScanStatus status{ScanStatus::EXHAUSTED};
    std::uint32_t nonce{};
    UInt256 hash{};
    std::uint64_t hashes_checked{};
};

class NonceScanner
{
public:
    NonceScanner();

    ScanResult Scan(
        const BlockHeader& header,
        const UInt256& target,
        std::uint32_t nonce_begin,
        std::uint32_t nonce_end,
        const std::atomic_bool* cancelled = nullptr,
        const std::atomic_bool* cancelled_secondary = nullptr,
        const std::atomic_bool* cancelled_tertiary = nullptr,
        std::atomic<std::uint64_t>* live_hashes = nullptr);

    std::size_t ScratchpadSize() const noexcept
    {
        return m_scratchpad.size();
    }

private:
    std::vector<unsigned char> m_scratchpad;
};

} // namespace mercaminer

#endif // MERCAMINER_SCANNER_H
