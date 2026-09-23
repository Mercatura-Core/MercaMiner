// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_MINING_JOB_H
#define MERCAMINER_MINING_JOB_H

#include <block_builder.h>
#include <serialization.h>

#include <cstdint>
#include <stdexcept>

namespace mercaminer {

class MiningJobException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct MiningCandidate
{
    BlockCandidate candidate;
    std::uint64_t extranonce{};
};

Bytes EncodeExtranonce64(
    std::uint64_t value);

class MiningJob
{
public:
    MiningJob(
        BlockTemplate block_template,
        Bytes payout_script,
        std::uint64_t initial_extranonce);

    MiningCandidate NextCandidate();

    bool HasMoreCandidates() const noexcept
    {
        return !m_extranonce_exhausted;
    }

    const BlockTemplate& Template() const noexcept
    {
        return m_block_template;
    }

private:
    BlockTemplate m_block_template;
    Bytes m_payout_script;
    std::uint64_t m_next_extranonce{};
    bool m_extranonce_exhausted{false};
};

} // namespace mercaminer

#endif // MERCAMINER_MINING_JOB_H
