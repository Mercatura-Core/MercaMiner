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
    using mercaminer::ParseMinerReportInterval;
    using mercaminer::ParseMinerThreadCount;
    using mercaminer::ResolveMinerConfig;
    using mercaminer::SelectDefaultMinerThreadCount;

    bool ok{true};

    const auto config =
        ParseMinerConfigText(
            R"(
# MercaMiner test configuration

network = regtest
payout_address = mcrt1ztest
threads = 6
block_limit = 0
report_interval = 15
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
            config.report_interval &&
            *config.report_interval == 15 &&
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
            "payout_address=mcrt1ztest\n");

    ok &= Check(
        minimal.network &&
            minimal.payout_address &&
            !minimal.thread_count &&
            !minimal.block_limit &&
            !minimal.report_interval &&
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
        SelectDefaultMinerThreadCount(0) == 1 &&
            SelectDefaultMinerThreadCount(1) == 1 &&
            SelectDefaultMinerThreadCount(4) == 4 &&
            SelectDefaultMinerThreadCount(8) == 8 &&
            SelectDefaultMinerThreadCount(12) == 8 &&
            SelectDefaultMinerThreadCount(64) == 8,
        "default thread selector clamps hardware concurrency");

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
        ParseMinerReportInterval("0") == 0 &&
            ParseMinerReportInterval("30") == 30,
        "report interval parser accepts zero and positive integers");

    ok &= Check(
        ThrowsConfig([] {
            (void)ParseMinerReportInterval("-1");
        }) &&
            ThrowsConfig([] {
                (void)ParseMinerReportInterval("abc");
            }) &&
            ThrowsConfig([] {
                (void)ParseMinerReportInterval(
                    "4294967296");
            }),
        "invalid report intervals rejected");

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
            "--report-interval",
            "12",
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
            command_line.overrides.report_interval &&
            *command_line.overrides.report_interval == 12 &&
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
    base.report_interval = 30;

    MinerConfig overrides;
    overrides.payout_address = "mcrt1zoverride";
    overrides.thread_count = 6;
    overrides.block_limit = 9;
    overrides.report_interval = 5;

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
            *merged.block_limit == 9 &&
            merged.report_interval &&
            *merged.report_interval == 5,
        "command-line values override config values");

    MinerConfig disable_report_override;
    disable_report_override.report_interval = 0;

    const MinerConfig disabled_merged =
        MergeMinerConfig(
            base,
            disable_report_override);

    ok &= Check(
        disabled_merged.report_interval &&
            *disabled_merged.report_interval == 0,
        "zero report interval override is preserved");

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
            resolved.report_interval == 30 &&
            !resolved.rpc_url &&
            !resolved.cookie_file,
        "resolved config applies continuous default");

    MinerConfig auto_threads_input;
    auto_threads_input.network = "regtest";
    auto_threads_input.payout_address =
        "mcrt1zauto";

    const auto resolved_auto_threads =
        ResolveMinerConfig(
            auto_threads_input,
            12);

    ok &= Check(
        resolved_auto_threads.thread_count == 8,
        "resolved config applies bounded automatic thread default");

    const auto resolved_unknown_hardware =
        ResolveMinerConfig(
            auto_threads_input,
            0);

    ok &= Check(
        resolved_unknown_hardware.thread_count == 1,
        "unknown hardware concurrency falls back to one thread");

    resolve_input.report_interval = 0;

    const auto resolved_disabled_reporter =
        ResolveMinerConfig(
            resolve_input);

    ok &= Check(
        resolved_disabled_reporter.report_interval == 0,
        "zero report interval disables periodic reporting");

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
