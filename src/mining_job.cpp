// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <mining_job.h>

#include <limits>
#include <utility>

namespace mercaminer {

Bytes EncodeExtranonce64(
    std::uint64_t value)
{
    Bytes result(8);

    for (std::size_t i = 0;
         i < result.size();
         ++i) {
        result[i] =
            static_cast<unsigned char>(
                value >> (8 * i));
    }

    return result;
}

MiningJob::MiningJob(
    BlockTemplate block_template,
    Bytes payout_script,
    std::uint64_t initial_extranonce)
    : m_block_template(
          std::move(block_template)),
      m_payout_script(
          std::move(payout_script)),
      m_next_extranonce(
          initial_extranonce)
{
    if (m_payout_script.empty()) {
        throw MiningJobException(
            "mining payout script must not be empty");
    }
}

MiningCandidate MiningJob::NextCandidate()
{
    if (m_extranonce_exhausted) {
        throw MiningJobException(
            "mining extranonce space exhausted");
    }

    const std::uint64_t extranonce =
        m_next_extranonce;

    const Bytes encoded_extranonce =
        EncodeExtranonce64(
            extranonce);

    MiningCandidate work{
        BuildBlockCandidate(
            m_block_template,
            m_payout_script,
            encoded_extranonce),
        extranonce};

    if (m_next_extranonce ==
        std::numeric_limits<std::uint64_t>::max()) {
        m_extranonce_exhausted = true;
    } else {
        ++m_next_extranonce;
    }

    return work;
}

} // namespace mercaminer
