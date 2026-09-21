// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_MINER_CONFIG_H
#define MERCAMINER_MINER_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mercaminer {

class MinerConfigException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct MinerConfig
{
    std::optional<std::string> network;
    std::optional<std::string> payout_address;
    std::optional<std::size_t> thread_count;
    std::optional<std::uint64_t> block_limit;
    std::optional<std::string> rpc_url;
    std::optional<std::string> cookie_file;
};

std::size_t ParseMinerThreadCount(
    std::string_view text);

std::uint64_t ParseMinerBlockLimit(
    std::string_view text);

MinerConfig ParseMinerConfigText(
    std::string_view text);

MinerConfig LoadMinerConfigFile(
    const std::filesystem::path& path);

std::filesystem::path MinerConfigPathForHome(
    std::string_view home_directory);

std::filesystem::path DefaultMinerConfigPath();

} // namespace mercaminer

#endif // MERCAMINER_MINER_CONFIG_H
