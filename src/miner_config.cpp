// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <miner_config.h>

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

namespace mercaminer {
namespace {

std::string_view Trim(std::string_view value)
{
    constexpr std::string_view whitespace{
        " \t\r\n"};

    const std::size_t first =
        value.find_first_not_of(whitespace);

    if (first == std::string_view::npos) {
        return {};
    }

    const std::size_t last =
        value.find_last_not_of(whitespace);

    return value.substr(
        first,
        last - first + 1);
}

[[noreturn]] void ConfigLineError(
    std::size_t line,
    const std::string& message)
{
    throw MinerConfigException(
        "mercaminer.conf line " +
        std::to_string(line) +
        ": " +
        message);
}

std::uint64_t ParseUnsigned(
    std::string_view text,
    const char* description)
{
    if (text.empty()) {
        throw MinerConfigException(
            std::string{description} +
            " must not be empty");
    }

    std::uint64_t value{};

    const char* begin = text.data();
    const char* end =
        text.data() + text.size();

    const auto result =
        std::from_chars(
            begin,
            end,
            value);

    if (result.ec != std::errc{} ||
        result.ptr != end) {
        throw MinerConfigException(
            std::string{description} +
            " must be an unsigned integer");
    }

    return value;
}

} // namespace

std::size_t ParseMinerThreadCount(
    std::string_view text)
{
    const std::uint64_t value =
        ParseUnsigned(
            text,
            "thread count");

    if (value == 0 ||
        value >
            std::numeric_limits<std::size_t>::max()) {
        throw MinerConfigException(
            "thread count must be a positive integer");
    }

    return static_cast<std::size_t>(
        value);
}

std::uint64_t ParseMinerBlockLimit(
    std::string_view text)
{
    return ParseUnsigned(
        text,
        "block limit");
}

MinerConfig ParseMinerConfigText(
    std::string_view text)
{
    MinerConfig config;
    std::set<std::string> seen;

    std::istringstream input{
        std::string{text}};

    std::string raw_line;
    std::size_t line_number{0};

    while (std::getline(input, raw_line)) {
        ++line_number;

        const std::string_view line =
            Trim(raw_line);

        if (line.empty() ||
            line.front() == '#') {
            continue;
        }

        const std::size_t separator =
            line.find('=');

        if (separator == std::string_view::npos) {
            ConfigLineError(
                line_number,
                "expected key=value");
        }

        const std::string_view key_view =
            Trim(line.substr(0, separator));

        const std::string_view value =
            Trim(line.substr(separator + 1));

        if (key_view.empty()) {
            ConfigLineError(
                line_number,
                "configuration key is empty");
        }

        if (value.empty()) {
            ConfigLineError(
                line_number,
                "configuration value is empty");
        }

        const std::string key{key_view};

        if (!seen.insert(key).second) {
            ConfigLineError(
                line_number,
                "duplicate configuration key '" +
                    key + "'");
        }

        try {
            if (key == "network") {
                config.network =
                    std::string{value};
            } else if (key == "payout_address") {
                config.payout_address =
                    std::string{value};
            } else if (key == "threads") {
                config.thread_count =
                    ParseMinerThreadCount(
                        value);
            } else if (key == "block_limit") {
                config.block_limit =
                    ParseMinerBlockLimit(
                        value);
            } else if (key == "rpc_url") {
                config.rpc_url =
                    std::string{value};
            } else if (key == "cookie_file") {
                config.cookie_file =
                    std::string{value};
            } else {
                throw MinerConfigException(
                    "unknown configuration key '" +
                    key + "'");
            }
        } catch (const MinerConfigException& error) {
            ConfigLineError(
                line_number,
                error.what());
        }
    }

    return config;
}

MinerConfig LoadMinerConfigFile(
    const std::filesystem::path& path)
{
    std::ifstream file{path};

    if (!file) {
        throw MinerConfigException(
            "unable to open MercaMiner configuration file: " +
            path.string());
    }

    std::ostringstream contents;
    contents << file.rdbuf();

    if (!file.eof() && file.fail()) {
        throw MinerConfigException(
            "unable to read MercaMiner configuration file: " +
            path.string());
    }

    return ParseMinerConfigText(
        contents.str());
}

std::filesystem::path MinerConfigPathForHome(
    std::string_view home_directory)
{
    if (home_directory.empty()) {
        throw MinerConfigException(
            "HOME directory is empty; use --config to specify a configuration file");
    }

    std::filesystem::path path{
        std::string{home_directory}};

    path /= ".mercaminer";
    path /= "mercaminer.conf";

    return path;
}

std::filesystem::path DefaultMinerConfigPath()
{
    const char* home =
        std::getenv("HOME");

    if (home == nullptr ||
        *home == '\0') {
        throw MinerConfigException(
            "HOME is not set; use --config to specify a configuration file");
    }

    return MinerConfigPathForHome(home);
}

} // namespace mercaminer
