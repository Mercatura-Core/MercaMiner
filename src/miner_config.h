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
#include <vector>

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
    std::optional<std::uint32_t> report_interval;
    std::optional<std::string> rpc_url;
    std::optional<std::string> cookie_file;
};

struct MinerCommandLine
{
    std::optional<std::filesystem::path> config_file;
    MinerConfig overrides;
    bool show_help{false};
};

struct ResolvedMinerConfig
{
    std::string network;
    std::string payout_address;
    std::size_t thread_count{};
    std::uint64_t block_limit{};
    std::uint32_t report_interval{};
    std::optional<std::string> rpc_url;
    std::optional<std::string> cookie_file;
};

std::size_t ParseMinerThreadCount(
    std::string_view text);

std::size_t SelectDefaultMinerThreadCount(
    std::size_t hardware_threads);

std::uint64_t ParseMinerBlockLimit(
    std::string_view text);

std::uint32_t ParseMinerReportInterval(
    std::string_view text);

MinerCommandLine ParseMinerCommandLine(
    const std::vector<std::string_view>& args);

MinerConfig MergeMinerConfig(
    MinerConfig base,
    const MinerConfig& overrides);

ResolvedMinerConfig ResolveMinerConfig(
    const MinerConfig& config);

ResolvedMinerConfig ResolveMinerConfig(
    const MinerConfig& config,
    std::size_t hardware_threads);

MinerConfig ParseMinerConfigText(
    std::string_view text);

MinerConfig LoadMinerConfigFile(
    const std::filesystem::path& path);

std::filesystem::path MinerConfigPathForHome(
    std::string_view home_directory);

std::filesystem::path DefaultMinerConfigPath();

} // namespace mercaminer

#endif // MERCAMINER_MINER_CONFIG_H
