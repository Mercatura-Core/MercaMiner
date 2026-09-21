// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_GBT_H
#define MERCAMINER_GBT_H

#include <uint256.h>

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mercaminer {

class TemplateException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct BlockchainInfo
{
    std::string chain;
    std::uint64_t blocks{};
    std::uint64_t headers{};
    UInt256 best_block_hash{};
    std::uint32_t bits{};
    UInt256 target{};
};

struct TemplateTransaction
{
    std::string data_hex;
    UInt256 txid{};
    UInt256 wtxid{};
    std::vector<std::size_t> depends;
    std::int64_t fee{};
    std::uint64_t sigops{};
    std::uint64_t weight{};
};

struct BlockTemplate
{
    std::int32_t version{};
    std::vector<std::string> rules;
    UInt256 previous_block_hash{};
    std::vector<TemplateTransaction> transactions;

    std::string coinbase_aux_flags;
    std::uint64_t coinbase_value{};
    std::string longpoll_id;

    UInt256 target{};
    std::uint32_t bits{};

    std::uint32_t minimum_time{};
    std::uint32_t current_time{};

    std::uint64_t height{};

    std::uint32_t nonce_min{};
    std::uint32_t nonce_max{};

    std::uint64_t sigop_limit{};
    std::uint64_t size_limit{};
    std::optional<std::uint64_t> weight_limit;

    std::string witness_commitment_hex;
};

BlockchainInfo ParseBlockchainInfo(
    const nlohmann::json& value);

BlockTemplate ParseBlockTemplate(
    const nlohmann::json& value);

bool HasTemplateRule(
    const BlockTemplate& block_template,
    const std::string& rule);

} // namespace mercaminer

#endif // MERCAMINER_GBT_H
