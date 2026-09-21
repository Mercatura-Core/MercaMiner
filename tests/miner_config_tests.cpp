// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <miner_config.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool Check(bool condition, const char* name)
{
    if (!condition) {
        std::cerr << "FAIL " << name << '\n';
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

template <typename Callable>
bool ThrowsConfig(Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::MinerConfigException&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    using mercaminer::LoadMinerConfigFile;
    using mercaminer::MergeMinerConfig;
    using mercaminer::MinerConfig;
    using mercaminer::MinerConfigPathForHome;
    using mercaminer::ParseMinerBlockLimit;
    using mercaminer::ParseMinerCommandLine;
    using mercaminer::ParseMinerConfigText;
    using mercaminer::ParseMinerThreadCount;
    using mercaminer::ResolveMinerConfig;

    bool ok{true};

    const auto config =
        ParseMinerConfigText(
            R"(
# MercaMiner test configuration

network = regtest
payout_address = mcrt1ztest
threads = 6
block_limit = 0
rpc_url = http://127.0.0.1:27773
cookie_file = /tmp/mercatura/regtest/.cookie
)");

    ok &= Check(
        config.network &&
            *config.network == "regtest" &&
            config.payout_address &&
            *config.payout_address == "mcrt1ztest" &&
            config.thread_count &&
            *config.thread_count == 6 &&
            config.block_limit &&
            *config.block_limit == 0 &&
            config.rpc_url &&
            *config.rpc_url ==
                "http://127.0.0.1:27773" &&
            config.cookie_file &&
            *config.cookie_file ==
                "/tmp/mercatura/regtest/.cookie",
        "complete configuration parsed");

    const auto minimal =
        ParseMinerConfigText(
            "network=regtest\n"
            "payout_address=mcrt1ztest\n"
            "threads=2\n");

    ok &= Check(
        minimal.network &&
            minimal.payout_address &&
            minimal.thread_count &&
            !minimal.block_limit &&
            !minimal.rpc_url &&
            !minimal.cookie_file,
        "optional configuration fields may be omitted");

    ok &= Check(
        ParseMinerThreadCount("1") == 1 &&
            ParseMinerThreadCount("12") == 12,
        "thread count parser accepts positive integers");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerThreadCount("0");
        }) &&
            ThrowsConfig([] {
                (void)ParseMinerThreadCount("-1");
            }) &&
            ThrowsConfig([] {
                (void)ParseMinerThreadCount("abc");
            }),
        "invalid thread counts rejected");

    ok &= Check(
        ParseMinerBlockLimit("0") == 0 &&
            ParseMinerBlockLimit("25") == 25,
        "block limit parser accepts zero and positive integers");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerBlockLimit("-1");
        }) &&
            ThrowsConfig([] {
                (void)ParseMinerBlockLimit("abc");
            }),
        "invalid block limits rejected");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerConfigText(
                "network=regtest\n"
                "network=main\n");
        }),
        "duplicate configuration key rejected");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerConfigText(
                "unknown_key=value\n");
        }),
        "unknown configuration key rejected");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerConfigText(
                "network regtest\n");
        }),
        "malformed configuration line rejected");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerConfigText(
                "network=\n");
        }),
        "empty configuration value rejected");

    ok &= Check(
        MinerConfigPathForHome(
            "/home/tester") ==
            std::filesystem::path{
                "/home/tester/.mercaminer/mercaminer.conf"},
        "default per-user configuration path");

    ok &= Check(
        ThrowsConfig([] {
            (void)MinerConfigPathForHome("");
        }),
        "empty HOME rejected");

    const std::filesystem::path temp =
        std::filesystem::temp_directory_path() /
        "mercaminer_config_test.conf";

    {
        std::ofstream file{temp};
        file
            << "network=regtest\n"
            << "payout_address=mcrt1zfile\n"
            << "threads=4\n"
            << "block_limit=7\n";
    }

    const auto loaded =
        LoadMinerConfigFile(temp);

    ok &= Check(
        loaded.network &&
            *loaded.network == "regtest" &&
            loaded.payout_address &&
            *loaded.payout_address == "mcrt1zfile" &&
            loaded.thread_count &&
            *loaded.thread_count == 4 &&
            loaded.block_limit &&
            *loaded.block_limit == 7,
        "configuration file loaded");

    std::filesystem::remove(temp);

    ok &= Check(
        ThrowsConfig([&] {
            (void)LoadMinerConfigFile(temp);
        }),
        "missing configuration file rejected");

    const auto command_line =
        ParseMinerCommandLine({
            "--config",
            "/tmp/custom.conf",
            "--network",
            "regtest",
            "--payout-address",
            "mcrt1zcli",
            "--threads",
            "8",
            "--block-limit",
            "3",
            "--rpc-url",
            "http://127.0.0.1:27773",
            "--cookie-file",
            "/tmp/regtest/.cookie"});

    ok &= Check(
        command_line.config_file &&
            *command_line.config_file ==
                std::filesystem::path{
                    "/tmp/custom.conf"} &&
            command_line.overrides.network &&
            *command_line.overrides.network ==
                "regtest" &&
            command_line.overrides.payout_address &&
            *command_line.overrides.payout_address ==
                "mcrt1zcli" &&
            command_line.overrides.thread_count &&
            *command_line.overrides.thread_count == 8 &&
            command_line.overrides.block_limit &&
            *command_line.overrides.block_limit == 3 &&
            command_line.overrides.rpc_url &&
            command_line.overrides.cookie_file,
        "command-line overrides parsed");

    const auto help =
        ParseMinerCommandLine(
            {"--help"});

    ok &= Check(
        help.show_help,
        "help option parsed");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerCommandLine(
                {"--threads", "2",
                 "--threads", "4"});
        }),
        "duplicate command-line option rejected");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerCommandLine(
                {"--threads"});
        }),
        "missing command-line option value rejected");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerCommandLine(
                {"--unknown", "value"});
        }),
        "unknown command-line option rejected");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerCommandLine(
                {"regtest"});
        }),
        "unexpected positional argument rejected");

    MinerConfig base;
    base.network = "regtest";
    base.payout_address = "mcrt1zbase";
    base.thread_count = 2;
    base.block_limit = 0;

    MinerConfig overrides;
    overrides.payout_address = "mcrt1zoverride";
    overrides.thread_count = 6;
    overrides.block_limit = 9;

    const MinerConfig merged =
        MergeMinerConfig(
            base,
            overrides);

    ok &= Check(
        merged.network &&
            *merged.network == "regtest" &&
            merged.payout_address &&
            *merged.payout_address ==
                "mcrt1zoverride" &&
            merged.thread_count &&
            *merged.thread_count == 6 &&
            merged.block_limit &&
            *merged.block_limit == 9,
        "command-line values override config values");

    MinerConfig resolve_input;
    resolve_input.network = "regtest";
    resolve_input.payout_address =
        "mcrt1zresolved";
    resolve_input.thread_count = 4;

    const auto resolved =
        ResolveMinerConfig(
            resolve_input);

    ok &= Check(
        resolved.network == "regtest" &&
            resolved.payout_address ==
                "mcrt1zresolved" &&
            resolved.thread_count == 4 &&
            resolved.block_limit == 0 &&
            !resolved.rpc_url &&
            !resolved.cookie_file,
        "resolved config applies continuous default");

    ok &= Check(
        ThrowsConfig([] {
            MinerConfig incomplete;
            incomplete.network = "regtest";
            incomplete.thread_count = 2;
            (void)ResolveMinerConfig(
                incomplete);
        }),
        "missing required payout rejected");

    ok &= Check(
        ThrowsConfig([] {
            MinerConfig incomplete;
            incomplete.network = "regtest";
            incomplete.payout_address =
                "mcrt1ztest";
            incomplete.thread_count = 2;
            incomplete.rpc_url =
                "http://127.0.0.1:27773";
            (void)ResolveMinerConfig(
                incomplete);
        }),
        "partial RPC override rejected");

    return ok ? 0 : 1;
}
