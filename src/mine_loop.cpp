// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <candidate_miner.h>
#include <gbt.h>
#include <hash256.h>
#include <longpoll.h>
#include <miner_config.h>
#include <mining_job.h>
#include <network.h>
#include <payout_address.h>
#include <parallel_miner.h>
#include <rpc.h>
#include <rpc_connection.h>
#include <runtime_reporter.h>
#include <scanner.h>
#include <serialization.h>
#include <uint256.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <syncstream>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

volatile std::sig_atomic_t g_shutdown_signal{0};

void HandleShutdownSignal(int signal) noexcept
{
    if (g_shutdown_signal == 0) {
        g_shutdown_signal = signal;
    }
}

class ShutdownMonitor
{
public:
    explicit ShutdownMonitor(
        std::atomic_bool& shutdown)
        : m_thread(
              [&shutdown](
                  std::stop_token token) {
                  while (!token.stop_requested()) {
                      if (g_shutdown_signal != 0) {
                          shutdown.store(
                              true,
                              std::memory_order_relaxed);
                          return;
                      }

                      std::this_thread::sleep_for(
                          std::chrono::milliseconds{10});
                  }
              })
    {
    }

private:
    std::jthread m_thread;
};

bool InterruptibleSleep(
    std::chrono::milliseconds duration,
    const std::atomic_bool* cancelled)
{
    constexpr auto STEP =
        std::chrono::milliseconds{10};

    auto remaining = duration;

    while (remaining.count() > 0) {
        if (cancelled != nullptr &&
            cancelled->load(
                std::memory_order_relaxed)) {
            return false;
        }

        const auto delay =
            remaining < STEP
                ? remaining
                : STEP;

        std::this_thread::sleep_for(delay);
        remaining -= delay;
    }

    return true;
}

mercaminer::RpcCallOptions
ShutdownRpcOptions(
    const std::atomic_bool* cancelled)
{
    mercaminer::RpcCallOptions options;
    options.cancelled = cancelled;
    return options;
}

void TryReloadRpcCookie(
    const mercaminer::RpcConnectionSettings& connection,
    mercaminer::RpcCredentials& credentials,
    mercaminer::RpcClient& rpc)
{
    try {
        if (mercaminer::ReloadRpcClientFromCookie(
                connection,
                credentials,
                rpc)) {
            std::osyncstream(std::cerr)
                << "RPC cookie credentials changed; "
                << "reloaded credentials\n";
        }
    } catch (const mercaminer::RpcException& error) {
        std::osyncstream(std::cerr)
            << "Unable to reload RPC cookie credentials: "
            << error.what()
            << "; retaining previous credentials\n";
    }
}

std::string Hex(
    const mercaminer::Bytes& bytes)
{
    static constexpr char DIGITS[] =
        "0123456789abcdef";

    std::string out;
    out.resize(bytes.size() * 2);

    for (std::size_t i = 0;
         i < bytes.size();
         ++i) {
        out[2 * i] =
            DIGITS[bytes[i] >> 4];

        out[2 * i + 1] =
            DIGITS[bytes[i] & 0x0f];
    }

    return out;
}

mercaminer::UInt256 ParseRpcHash(
    const nlohmann::json& value,
    const char* description)
{
    if (!value.is_string()) {
        throw std::runtime_error(
            std::string{description} +
            " RPC result is not a string");
    }

    const auto hash =
        mercaminer::UInt256::FromHexBE(
            value.get<std::string>());

    if (!hash) {
        throw std::runtime_error(
            std::string{description} +
            " RPC result is not a 256-bit hash");
    }

    return *hash;
}

struct StartupRpcState
{
    mercaminer::RpcCredentials credentials;
    mercaminer::RpcClient rpc;
    mercaminer::Bytes payout_script;
};

std::optional<StartupRpcState> WaitForStartupRpc(
    const mercaminer::NetworkIdentity& network,
    const mercaminer::RpcConnectionSettings& connection,
    std::string_view payout_address,
    const std::atomic_bool* cancelled)
{
    for (;;) {
        if (cancelled != nullptr &&
            cancelled->load(
                std::memory_order_relaxed)) {
            return std::nullopt;
        }

        try {
            mercaminer::RpcCredentials credentials =
                mercaminer::RpcCredentials::FromCookieFile(
                    connection.cookie_file);

            mercaminer::RpcClient rpc{
                connection.rpc_url,
                credentials};

            const auto options =
                ShutdownRpcOptions(cancelled);

            const auto initial_blockchain =
                mercaminer::ParseBlockchainInfo(
                    rpc.Call(
                        "getblockchaininfo",
                        nlohmann::json::array(),
                        options));

            const auto live_genesis =
                ParseRpcHash(
                    rpc.Call(
                        "getblockhash",
                        nlohmann::json::array({0}),
                        options),
                    "getblockhash 0");

            mercaminer::ValidateNetworkIdentity(
                network,
                initial_blockchain,
                live_genesis);

            mercaminer::Bytes payout_script =
                mercaminer::ResolvePayoutAddress(
                    rpc,
                    payout_address,
                    options);

            return StartupRpcState{
                std::move(credentials),
                std::move(rpc),
                std::move(payout_script)};
        } catch (
            const mercaminer::RpcCancelledException&) {
            if (cancelled != nullptr &&
                cancelled->load(
                    std::memory_order_relaxed)) {
                return std::nullopt;
            }

            throw;
        } catch (const mercaminer::RpcException& error) {
            std::osyncstream(std::cerr)
                << "RPC startup connection failed: "
                << error.what()
                << '\n'
                << "Retrying startup in 1 second\n";

            if (!InterruptibleSleep(
                    std::chrono::seconds{1},
                    cancelled)) {
                return std::nullopt;
            }
        }
    }
}

std::uint64_t ParseRpcHeight(
    const nlohmann::json& value,
    const char* description)
{
    if (!value.is_number_integer() &&
        !value.is_number_unsigned()) {
        throw std::runtime_error(
            std::string{description} +
            " RPC result is not an integer");
    }

    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>();
    }

    const std::int64_t height =
        value.get<std::int64_t>();

    if (height < 0) {
        throw std::runtime_error(
            std::string{description} +
            " RPC result is negative");
    }

    return static_cast<std::uint64_t>(
        height);
}


nlohmann::json TemplateRequest()
{
    return nlohmann::json{
        {
            "rules",
            nlohmann::json::array(
                {"segwit"})
        }
    };
}

struct CurrentWork
{
    mercaminer::BlockchainInfo blockchain;
    mercaminer::BlockTemplate block_template;
};

CurrentWork FetchCurrentWork(
    mercaminer::RpcClient& rpc,
    const mercaminer::NetworkIdentity& network,
    const std::atomic_bool* cancelled)
{
    const auto options =
        ShutdownRpcOptions(cancelled);

    CurrentWork work;

    work.blockchain =
        mercaminer::ParseBlockchainInfo(
            rpc.Call(
                "getblockchaininfo",
                nlohmann::json::array(),
                options));

    work.block_template =
        mercaminer::ParseBlockTemplate(
            rpc.Call(
                "getblocktemplate",
                nlohmann::json::array(
                    {TemplateRequest()}),
                options));

    mercaminer::ValidateProofOfWorkTarget(
        network,
        work.block_template.target);

    return work;
}

bool TemplateMatchesTip(
    const CurrentWork& work)
{
    if (work.blockchain.blocks ==
        std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }

    return
        work.block_template.previous_block_hash ==
            work.blockchain.best_block_hash &&
        work.block_template.height ==
            work.blockchain.blocks + 1;
}

enum class SubmitOutcome
{
    ACCEPTED,
    DUPLICATE,
    REJECTED,
};

enum class ResolvedSubmitOutcome
{
    ACCEPTED,
    REJECTED,
    NOT_CURRENT_TIP,
    CANCELLED,
};

SubmitOutcome SubmitCandidate(
    mercaminer::RpcClient& rpc,
    const mercaminer::BlockCandidate& candidate,
    std::string& rejection,
    const std::atomic_bool* cancelled)
{
    const auto result =
        rpc.Call(
            "submitblock",
            nlohmann::json::array(
                {Hex(candidate.serialized_block)}),
            ShutdownRpcOptions(cancelled));

    if (result.is_null()) {
        return SubmitOutcome::ACCEPTED;
    }

    if (!result.is_string()) {
        throw std::runtime_error(
            "submitblock returned unexpected "
            "non-null result");
    }

    rejection =
        result.get<std::string>();

    if (rejection == "duplicate") {
        return SubmitOutcome::DUPLICATE;
    }

    return SubmitOutcome::REJECTED;
}

bool ConfirmAcceptedTip(
    mercaminer::RpcClient& rpc,
    std::uint64_t expected_height,
    const mercaminer::UInt256& expected_hash,
    const std::atomic_bool* cancelled)
{
    const auto options =
        ShutdownRpcOptions(cancelled);

    const std::uint64_t height =
        ParseRpcHeight(
            rpc.Call(
                "getblockcount",
                nlohmann::json::array(),
                options),
            "getblockcount");

    const auto best =
        ParseRpcHash(
            rpc.Call(
                "getbestblockhash",
                nlohmann::json::array(),
                options),
            "getbestblockhash");

    return
        height == expected_height &&
        best == expected_hash;
}


ResolvedSubmitOutcome SubmitCandidateReliably(
    mercaminer::RpcClient& rpc,
    const mercaminer::RpcConnectionSettings& connection,
    mercaminer::RpcCredentials& credentials,
    const mercaminer::BlockCandidate& candidate,
    std::uint64_t expected_height,
    const mercaminer::UInt256& expected_hash,
    std::string& rejection,
    const std::atomic_bool* cancelled)
{
    for (;;) {
        if (cancelled != nullptr &&
            cancelled->load(
                std::memory_order_relaxed)) {
            return
                ResolvedSubmitOutcome::CANCELLED;
        }

        try {
            rejection.clear();

            const SubmitOutcome outcome =
                SubmitCandidate(
                    rpc,
                    candidate,
                    rejection,
                    cancelled);

            if (outcome ==
                SubmitOutcome::REJECTED) {
                return
                    ResolvedSubmitOutcome::REJECTED;
            }

            if (ConfirmAcceptedTip(
                    rpc,
                    expected_height,
                    expected_hash,
                    cancelled)) {
                return
                    ResolvedSubmitOutcome::ACCEPTED;
            }

            return
                ResolvedSubmitOutcome::NOT_CURRENT_TIP;
        } catch (
            const mercaminer::RpcCancelledException&) {
            if (cancelled != nullptr &&
                cancelled->load(
                    std::memory_order_relaxed)) {
                return
                    ResolvedSubmitOutcome::CANCELLED;
            }

            throw;
        } catch (const mercaminer::RpcException& error) {
            std::osyncstream(std::cerr)
                << "RPC error while submitting or confirming "
                << "solved candidate: "
                << error.what()
                << '\n';

            TryReloadRpcCookie(
                connection,
                credentials,
                rpc);

            std::osyncstream(std::cerr)
                << "Retrying the same solved candidate "
                << "in 1 second\n";

            if (!InterruptibleSleep(
                    std::chrono::seconds{1},
                    cancelled)) {
                return
                    ResolvedSubmitOutcome::CANCELLED;
            }
        }
    }
}

void PrintMiningSessionSummary(
    std::string_view title,
    const mercaminer::MiningRuntimeSnapshot& stats)
{
    std::osyncstream(std::cout)
        << title
        << '\n'
        << "  accepted blocks: "
        << stats.accepted_blocks
        << '\n'
        << "  stale work: "
        << stats.stale_work
        << '\n'
        << "  rejected blocks: "
        << stats.rejected_blocks
        << '\n'
        << "  session hashes checked: "
        << stats.completed_hashes
        << '\n';
}

void PrintMiningUsage(
    const char* program)
{
    std::osyncstream(std::cerr)
        << "Usage:\n"
        << "  " << program << "\n"
        << "  " << program
        << " --config <file> [options]\n"
        << "  " << program
        << " [--network <network>]"
        << " [--payout-address <address>]"
        << " [--threads <count>]"
        << " [--block-limit <count>]"
        << " [--report-interval <seconds>]"
        << " [--rpc-url <url>]"
        << " [--cookie-file <file>]\n"
        << "\nLegacy positional forms:\n"
        << "  " << program
        << " <network> <payout-address>"
        << " <thread-count> <block-count>\n"
        << "  " << program
        << " <network> <rpc-url> <cookie-file>"
        << " <payout-address> <thread-count>"
        << " <block-count>\n"
        << "\nDefault config:"
        << " ~/.mercaminer/mercaminer.conf\n"
        << "block-limit 0 means run continuously\n"
        << "report-interval 0 disables periodic status\n";
}

struct StartupArguments
{
    bool show_help{false};
    mercaminer::ResolvedMinerConfig config;
};

StartupArguments ResolveStartupArguments(
    int argc,
    char* argv[])
{
    const bool legacy_positional =
        (argc == 5 || argc == 7) &&
        argc > 1 &&
        !std::string_view{argv[1]}.starts_with("--");

    if (legacy_positional) {
        mercaminer::MinerConfig config;
        config.network =
            std::string{argv[1]};

        const bool explicit_rpc =
            argc == 7;

        config.payout_address =
            std::string{
                argv[explicit_rpc ? 4 : 2]};

        config.thread_count =
            mercaminer::ParseMinerThreadCount(
                argv[explicit_rpc ? 5 : 3]);

        config.block_limit =
            mercaminer::ParseMinerBlockLimit(
                argv[explicit_rpc ? 6 : 4]);

        if (explicit_rpc) {
            config.rpc_url =
                std::string{argv[2]};
            config.cookie_file =
                std::string{argv[3]};
        }

        return StartupArguments{
            false,
            mercaminer::ResolveMinerConfig(
                config)};
    }

    if (argc > 1 &&
        !std::string_view{argv[1]}.starts_with("--")) {
        throw mercaminer::MinerConfigException(
            "invalid positional arguments; use --help for supported forms");
    }

    std::vector<std::string_view> arguments;
    arguments.reserve(
        argc > 1
            ? static_cast<std::size_t>(argc - 1)
            : 0);

    for (int i = 1; i < argc; ++i) {
        arguments.emplace_back(
            argv[i]);
    }

    const auto command_line =
        mercaminer::ParseMinerCommandLine(
            arguments);

    if (command_line.show_help) {
        return StartupArguments{
            true,
            {}};
    }

    mercaminer::MinerConfig config;

    if (command_line.config_file) {
        config =
            mercaminer::LoadMinerConfigFile(
                *command_line.config_file);
    } else {
        const bool require_default =
            arguments.empty();

        std::optional<std::filesystem::path>
            default_path;

        try {
            default_path =
                mercaminer::DefaultMinerConfigPath();
        } catch (
            const mercaminer::MinerConfigException&) {
            if (require_default) {
                throw;
            }
        }

        if (default_path) {
            std::error_code error;
            const bool exists =
                std::filesystem::exists(
                    *default_path,
                    error);

            if (error) {
                throw mercaminer::MinerConfigException(
                    "unable to inspect default MercaMiner configuration file: " +
                    default_path->string());
            }

            if (exists) {
                config =
                    mercaminer::LoadMinerConfigFile(
                        *default_path);
            } else if (require_default) {
                throw mercaminer::MinerConfigException(
                    "default MercaMiner configuration file not found: " +
                    default_path->string());
            }
        }
    }

    config =
        mercaminer::MergeMinerConfig(
            std::move(config),
            command_line.overrides);

    return StartupArguments{
        false,
        mercaminer::ResolveMinerConfig(
            config)};
}

} // namespace

int main(int argc, char* argv[])
{
    using mercaminer::FindNetworkIdentity;
    using mercaminer::LongpollStatus;
    using mercaminer::LongpollWatcher;
    using mercaminer::MiningJob;
    using mercaminer::ParallelCandidateMiner;
    using mercaminer::RpcClient;
    using mercaminer::RpcConnectionSettings;
    using mercaminer::RpcCredentials;
    using mercaminer::RpcException;
    using mercaminer::ScanStatus;
    using mercaminer::ValidateNetworkIdentity;

    try {
        const StartupArguments startup =
            ResolveStartupArguments(
                argc,
                argv);

        if (startup.show_help) {
            PrintMiningUsage(
                argv[0]);
            return 0;
        }

        const auto& startup_config =
            startup.config;

        const std::string& network_name =
            startup_config.network;

        const std::string& payout_address =
            startup_config.payout_address;

        const std::size_t thread_count =
            startup_config.thread_count;

        const std::uint64_t block_limit =
            startup_config.block_limit;

        const std::uint32_t report_interval =
            startup_config.report_interval;

        const auto* network =
            FindNetworkIdentity(
                network_name);

        if (network == nullptr) {
            throw mercaminer::MinerConfigException(
                "unsupported Mercatura network '" +
                network_name + "'");
        }

        if (!mercaminer::SupportsDirectPowMining(
                *network)) {
            throw mercaminer::MinerConfigException(
                "Mercatura signet mining is not supported "
                "because signet challenge solutions are not implemented");
        }

        g_shutdown_signal = 0;

        if (std::signal(
                SIGINT,
                HandleShutdownSignal) == SIG_ERR ||
            std::signal(
                SIGTERM,
                HandleShutdownSignal) == SIG_ERR) {
            throw std::runtime_error(
                "unable to install shutdown signal handlers");
        }

        std::atomic_bool shutdown_requested{false};
        ShutdownMonitor shutdown_monitor{
            shutdown_requested};

        const RpcConnectionSettings connection =
            startup_config.rpc_url
                ? RpcConnectionSettings{
                      *startup_config.rpc_url,
                      *startup_config.cookie_file}
                : mercaminer::DefaultLocalRpcConnection(
                      *network);

        auto startup_rpc =
            WaitForStartupRpc(
                *network,
                connection,
                payout_address,
                &shutdown_requested);

        if (!startup_rpc) {
            const mercaminer::MiningRuntimeSnapshot
                empty_stats{};

            PrintMiningSessionSummary(
                "MercaMiner shutdown complete",
                empty_stats);

            const int signal =
                static_cast<int>(
                    g_shutdown_signal);

            return signal != 0
                ? 128 + signal
                : 0;
        }

        RpcCredentials credentials =
            std::move(
                startup_rpc->credentials);

        RpcClient rpc =
            std::move(
                startup_rpc->rpc);

        mercaminer::Bytes payout_script =
            std::move(
                startup_rpc->payout_script);

        ParallelCandidateMiner miner{
            thread_count};

        mercaminer::MiningRuntimeStats
            runtime_stats;

        {
            std::osyncstream startup{
                std::cout};

            startup
                << "MercaMiner mining started\n"
                << "  network: "
                << network_name
                << '\n'
                << "  payout address: "
                << payout_address
                << '\n'
                << "  worker threads: "
                << miner.WorkerCount()
                << '\n'
                << "  scratchpad memory: "
                << (miner.ScratchpadBytes() /
                    (1024ULL * 1024ULL))
                << " MiB\n"
                << "  block limit: ";

            if (block_limit == 0) {
                startup
                    << "continuous\n";
            } else {
                startup
                    << block_limit
                    << '\n';
            }

            startup
                << "  status interval: ";

            if (report_interval == 0) {
                startup
                    << "disabled\n";
            } else {
                startup
                    << report_interval
                    << (report_interval == 1
                        ? " second\n"
                        : " seconds\n");
            }
        }

        std::unique_ptr<mercaminer::RuntimeReporter>
            reporter;

        if (report_interval != 0) {
            reporter =
                std::make_unique<
                    mercaminer::RuntimeReporter>(
                        runtime_stats,
                        miner.WorkerCount(),
                        std::chrono::seconds{
                            static_cast<
                                std::chrono::seconds::rep>(
                                    report_interval)},
                        std::cout);
        }

        while (!shutdown_requested.load(
                   std::memory_order_relaxed) &&
               (block_limit == 0 ||
                runtime_stats.accepted_blocks.load(
                    std::memory_order_relaxed) <
                    block_limit)) {
            try {
                CurrentWork work =
                    FetchCurrentWork(
                        rpc,
                        *network,
                        &shutdown_requested);

                if (!TemplateMatchesTip(work)) {
                    std::osyncstream(std::cout)
                        << "Template changed during fetch; "
                        << "refreshing\n";

                    continue;
                }

                runtime_stats.current_height.store(
                    work.block_template.height,
                    std::memory_order_relaxed);

                MiningJob job{
                    work.block_template,
                    payout_script,
                    work.block_template.height};

                LongpollWatcher watcher{
                    connection.rpc_url,
                    credentials,
                    work.block_template.longpoll_id};

                bool refresh_template{false};

                while (job.HasMoreCandidates() &&
                       !refresh_template &&
                       !shutdown_requested.load(
                           std::memory_order_relaxed)) {
                    auto mining_candidate =
                        job.NextCandidate();

                    std::osyncstream(std::cout)
                        << "Mining height "
                        << work.block_template.height
                        << " extranonce "
                        << mining_candidate.extranonce
                        << '\n';

                    const auto result =
                        miner.Mine(
                            mining_candidate.candidate,
                            work.block_template.target,
                            work.block_template.nonce_min,
                            work.block_template.nonce_max,
                            watcher.StaleFlag(),
                            &shutdown_requested,
                            &runtime_stats.completed_hashes);

                    if (result.status ==
                        ScanStatus::CANCELLED) {
                        const LongpollStatus status =
                            watcher.Finish();

                        if (shutdown_requested.load(
                                std::memory_order_relaxed)) {
                            std::osyncstream(std::cout)
                                << "Shutdown requested; "
                                << "stopping mining workers\n";

                            refresh_template = true;
                            break;
                        }

                        if (status ==
                            LongpollStatus::RPC_ERROR) {
                            std::osyncstream(std::cerr)
                                << "Longpoll failed: "
                                << watcher.Error()
                                << "; refreshing template\n";
                        } else {
                            runtime_stats.stale_work.fetch_add(
                                1,
                                std::memory_order_relaxed);

                            std::osyncstream(std::cout)
                                << "Longpoll reported new work; "
                                << "discarding stale candidate\n";
                        }

                        refresh_template = true;
                        continue;
                    }

                    if (result.status ==
                        ScanStatus::EXHAUSTED) {
                        if (watcher.IsStale()) {
                            const LongpollStatus status =
                                watcher.Finish();

                            if (status ==
                                LongpollStatus::RPC_ERROR) {
                                std::osyncstream(std::cerr)
                                    << "Longpoll failed: "
                                    << watcher.Error()
                                    << "; refreshing template\n";
                            } else {
                                runtime_stats.stale_work.fetch_add(
                                    1,
                                    std::memory_order_relaxed);

                                std::osyncstream(std::cout)
                                    << "Longpoll reported new work; "
                                    << "refreshing template\n";
                            }

                            refresh_template = true;
                            continue;
                        }

                        std::osyncstream(std::cout)
                            << "Nonce range exhausted after "
                            << result.hashes_checked
                            << " hashes; advancing extranonce\n";

                        continue;
                    }

                    const LongpollStatus watcher_status =
                        watcher.Finish();

                    if (shutdown_requested.load(
                            std::memory_order_relaxed)) {
                        std::osyncstream(std::cout)
                            << "Shutdown requested before submission; "
                            << "discarding solved candidate\n";

                        refresh_template = true;
                        break;
                    }

                    if (watcher.IsStale()) {
                        if (watcher_status ==
                            LongpollStatus::RPC_ERROR) {
                            std::osyncstream(std::cerr)
                                << "Longpoll failed: "
                                << watcher.Error()
                                << "; discarding solved candidate "
                                << "and refreshing template\n";
                        } else {
                            runtime_stats.stale_work.fetch_add(
                                1,
                                std::memory_order_relaxed);

                            std::osyncstream(std::cout)
                                << "Template changed while solution "
                                << "was being found; discarding "
                                << "solved candidate\n";
                        }

                        refresh_template = true;
                        continue;
                    }

                    const auto solved_header =
                        mining_candidate.candidate.header.Serialize();

                    const auto expected_block_hash =
                        mercaminer::DoubleSha256(
                            solved_header);

                    std::string rejection;

                    const ResolvedSubmitOutcome outcome =
                        SubmitCandidateReliably(
                            rpc,
                            connection,
                            credentials,
                            mining_candidate.candidate,
                            work.block_template.height,
                            expected_block_hash,
                            rejection,
                            &shutdown_requested);

                    if (outcome ==
                        ResolvedSubmitOutcome::CANCELLED) {
                        std::osyncstream(std::cout)
                            << "Shutdown requested during submission\n";

                        refresh_template = true;
                        break;
                    }

                    if (outcome ==
                        ResolvedSubmitOutcome::REJECTED) {
                        runtime_stats.rejected_blocks.fetch_add(
                            1,
                            std::memory_order_relaxed);

                        std::osyncstream(std::cout)
                            << "submitblock rejected candidate: "
                            << rejection
                            << "; refreshing template\n";

                        refresh_template = true;
                        continue;
                    }

                    if (outcome ==
                        ResolvedSubmitOutcome::NOT_CURRENT_TIP) {
                        runtime_stats.stale_work.fetch_add(
                            1,
                            std::memory_order_relaxed);

                        std::osyncstream(std::cout)
                            << "Submitted candidate is not the "
                            << "current best tip; refreshing template\n";

                        refresh_template = true;
                        continue;
                    }

                    const std::uint64_t accepted_blocks =
                        runtime_stats.accepted_blocks.fetch_add(
                            1,
                            std::memory_order_relaxed) +
                        1;

                    std::osyncstream(std::cout)
                        << "Block accepted\n"
                        << "  height: "
                        << work.block_template.height
                        << '\n'
                        << "  nonce: "
                        << result.nonce
                        << '\n'
                        << "  extranonce: "
                        << mining_candidate.extranonce
                        << '\n'
                        << "  MercaHash: "
                        << result.hash.ToHexBE()
                        << '\n'
                        << "  block hash: "
                        << expected_block_hash.ToHexBE()
                        << '\n'
                        << "  active workers: "
                        << result.active_workers
                        << '\n'
                        << "  aggregate block hashes checked: "
                        << result.hashes_checked
                        << '\n'
                        << "  session hashes checked: "
                        << runtime_stats.completed_hashes.load(
                               std::memory_order_relaxed)
                        << '\n'
                        << "  accepted this session: "
                        << accepted_blocks
                        << '\n';

                    refresh_template = true;
                }

                if (!job.HasMoreCandidates() &&
                    !refresh_template &&
                    !shutdown_requested.load(
                        std::memory_order_relaxed)) {
                    (void)watcher.Finish();

                    throw std::runtime_error(
                        "64-bit extranonce space exhausted");
                }
            } catch (
                const mercaminer::RpcCancelledException&) {
                if (shutdown_requested.load(
                        std::memory_order_relaxed)) {
                    break;
                }

                throw;
            } catch (const RpcException& error) {
                if (shutdown_requested.load(
                        std::memory_order_relaxed)) {
                    break;
                }

                std::osyncstream(std::cerr)
                    << "RPC error: "
                    << error.what()
                    << '\n';

                TryReloadRpcCookie(
                    connection,
                    credentials,
                    rpc);

                std::osyncstream(std::cerr)
                    << "Retrying in 1 second\n";

                if (!InterruptibleSleep(
                        std::chrono::seconds{1},
                        &shutdown_requested)) {
                    break;
                }
            }
        }

        if (reporter) {
            reporter->Stop();
        }

        const auto final_stats =
            mercaminer::SnapshotMiningRuntimeStats(
                runtime_stats);

        if (shutdown_requested.load(
                std::memory_order_relaxed)) {
            PrintMiningSessionSummary(
                "MercaMiner shutdown complete",
                final_stats);

            const int signal =
                static_cast<int>(
                    g_shutdown_signal);

            return signal != 0
                ? 128 + signal
                : 0;
        }

        PrintMiningSessionSummary(
            "Requested block count reached",
            final_stats);

        return 0;
    } catch (const mercaminer::MinerConfigException& error) {
        std::osyncstream(std::cerr)
            << "MercaMiner configuration error: "
            << error.what()
            << '\n'
            << "Use '"
            << argv[0]
            << " --help' for usage.\n";

        return 2;
    } catch (const std::exception& error) {
        std::osyncstream(std::cerr)
            << "MercaMiner mining failed: "
            << error.what()
            << '\n';

        return 1;
    }
}
