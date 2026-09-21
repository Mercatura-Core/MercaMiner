// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <candidate_miner.h>
#include <gbt.h>
#include <hash256.h>
#include <longpoll.h>
#include <mining_job.h>
#include <network.h>
#include <payout_address.h>
#include <parallel_miner.h>
#include <rpc.h>
#include <rpc_connection.h>
#include <scanner.h>
#include <serialization.h>
#include <uint256.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

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

std::size_t ParseThreadCount(
    std::string_view text)
{
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
        result.ptr != end ||
        value == 0 ||
        value >
            std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "thread count must be a positive integer");
    }

    return static_cast<std::size_t>(
        value);
}

std::uint64_t ParseBlockLimit(
    std::string_view text)
{
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
        throw std::runtime_error(
            "block count must be an unsigned integer");
    }

    return value;
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
            std::cerr
                << "RPC error while submitting or confirming "
                << "solved candidate: "
                << error.what()
                << '\n'
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

    if (argc != 5 && argc != 7) {
        std::cerr
            << "Usage:\n"
            << "  " << argv[0]
            << " <network> <payout-address>"
            << " <thread-count> <block-count>\n"
            << "  " << argv[0]
            << " <network> <rpc-url> <cookie-file>"
            << " <payout-address> <thread-count>"
            << " <block-count>\n"
            << "thread-count must be at least 1\n"
            << "block-count 0 means run continuously\n";

        return 2;
    }

    const std::string network_name{argv[1]};
    const bool explicit_rpc = argc == 7;
    const std::string payout_address{
        argv[explicit_rpc ? 4 : 2]};

    if (network_name != "regtest") {
        std::cerr
            << "MercaMiner continuous mining is currently "
            << "restricted to regtest.\n";

        return 2;
    }

    try {
        const std::size_t thread_count =
            ParseThreadCount(
                argv[explicit_rpc ? 5 : 3]);

        const std::uint64_t block_limit =
            ParseBlockLimit(
                argv[explicit_rpc ? 6 : 4]);

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

        const auto* network =
            FindNetworkIdentity(
                network_name);

        if (network == nullptr) {
            throw std::runtime_error(
                "unsupported Mercatura network");
        }

        const RpcConnectionSettings connection =
            explicit_rpc
                ? RpcConnectionSettings{
                      argv[2],
                      argv[3]}
                : mercaminer::DefaultLocalRpcConnection(
                      *network);

        const RpcCredentials credentials =
            RpcCredentials::FromCookieFile(
                connection.cookie_file);

        RpcClient rpc{
            connection.rpc_url,
            credentials};

        mercaminer::Bytes payout_script;

        try {
            const auto startup_options =
                ShutdownRpcOptions(
                    &shutdown_requested);

            const auto initial_blockchain =
                mercaminer::ParseBlockchainInfo(
                    rpc.Call(
                        "getblockchaininfo",
                        nlohmann::json::array(),
                        startup_options));

            const auto live_genesis =
                ParseRpcHash(
                    rpc.Call(
                        "getblockhash",
                        nlohmann::json::array({0}),
                        startup_options),
                    "getblockhash 0");

            ValidateNetworkIdentity(
                *network,
                initial_blockchain,
                live_genesis);

            payout_script =
                mercaminer::ResolvePayoutAddress(
                    rpc,
                    payout_address,
                    startup_options);
        } catch (
            const mercaminer::RpcCancelledException&) {
            if (!shutdown_requested.load(
                    std::memory_order_relaxed)) {
                throw;
            }

            std::cout
                << "MercaMiner shutdown complete\n"
                << "  accepted blocks: 0\n"
                << "  aggregate hashes checked: 0\n";

            const int signal =
                static_cast<int>(
                    g_shutdown_signal);

            return signal != 0
                ? 128 + signal
                : 0;
        }

        ParallelCandidateMiner miner{
            thread_count};

        std::uint64_t accepted_blocks{0};
        std::uint64_t total_hashes{0};

        std::cout
            << "MercaMiner continuous regtest mining started\n"
            << "  worker threads: "
            << miner.WorkerCount()
            << '\n'
            << "  scratchpad memory: "
            << (miner.ScratchpadBytes() /
                (1024ULL * 1024ULL))
            << " MiB\n";

        if (block_limit == 0) {
            std::cout
                << "  block limit: continuous\n";
        } else {
            std::cout
                << "  block limit: "
                << block_limit
                << '\n';
        }

        while (!shutdown_requested.load(
                   std::memory_order_relaxed) &&
               (block_limit == 0 ||
                accepted_blocks < block_limit)) {
            try {
                CurrentWork work =
                    FetchCurrentWork(
                        rpc,
                        &shutdown_requested);

                if (!TemplateMatchesTip(work)) {
                    std::cout
                        << "Template changed during fetch; "
                        << "refreshing\n";

                    continue;
                }

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

                    std::cout
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
                            &shutdown_requested);

                    if (result.hashes_checked >
                        std::numeric_limits<std::uint64_t>::max() -
                            total_hashes) {
                        total_hashes =
                            std::numeric_limits<std::uint64_t>::max();
                    } else {
                        total_hashes +=
                            result.hashes_checked;
                    }

                    if (result.status ==
                        ScanStatus::CANCELLED) {
                        const LongpollStatus status =
                            watcher.Finish();

                        if (shutdown_requested.load(
                                std::memory_order_relaxed)) {
                            std::cout
                                << "Shutdown requested; "
                                << "stopping mining workers\n";

                            refresh_template = true;
                            break;
                        }

                        if (status ==
                            LongpollStatus::RPC_ERROR) {
                            std::cerr
                                << "Longpoll failed: "
                                << watcher.Error()
                                << "; refreshing template\n";
                        } else {
                            std::cout
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
                                std::cerr
                                    << "Longpoll failed: "
                                    << watcher.Error()
                                    << "; refreshing template\n";
                            } else {
                                std::cout
                                    << "Longpoll reported new work; "
                                    << "refreshing template\n";
                            }

                            refresh_template = true;
                            continue;
                        }

                        std::cout
                            << "Nonce range exhausted after "
                            << result.hashes_checked
                            << " hashes; advancing extranonce\n";

                        continue;
                    }

                    const LongpollStatus watcher_status =
                        watcher.Finish();

                    if (shutdown_requested.load(
                            std::memory_order_relaxed)) {
                        std::cout
                            << "Shutdown requested before submission; "
                            << "discarding solved candidate\n";

                        refresh_template = true;
                        break;
                    }

                    if (watcher.IsStale()) {
                        if (watcher_status ==
                            LongpollStatus::RPC_ERROR) {
                            std::cerr
                                << "Longpoll failed: "
                                << watcher.Error()
                                << "; discarding solved candidate "
                                << "and refreshing template\n";
                        } else {
                            std::cout
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
                            mining_candidate.candidate,
                            work.block_template.height,
                            expected_block_hash,
                            rejection,
                            &shutdown_requested);

                    if (outcome ==
                        ResolvedSubmitOutcome::CANCELLED) {
                        std::cout
                            << "Shutdown requested during submission\n";

                        refresh_template = true;
                        break;
                    }

                    if (outcome ==
                        ResolvedSubmitOutcome::REJECTED) {
                        std::cout
                            << "submitblock rejected candidate: "
                            << rejection
                            << "; refreshing template\n";

                        refresh_template = true;
                        continue;
                    }

                    if (outcome ==
                        ResolvedSubmitOutcome::NOT_CURRENT_TIP) {
                        std::cout
                            << "Submitted candidate is not the "
                            << "current best tip; refreshing template\n";

                        refresh_template = true;
                        continue;
                    }

                    ++accepted_blocks;

                    std::cout
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
                        << "  aggregate session hashes checked: "
                        << total_hashes
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

                std::cerr
                    << "RPC error: "
                    << error.what()
                    << "\nRetrying in 1 second\n";

                if (!InterruptibleSleep(
                        std::chrono::seconds{1},
                        &shutdown_requested)) {
                    break;
                }
            }
        }

        if (shutdown_requested.load(
                std::memory_order_relaxed)) {
            std::cout
                << "MercaMiner shutdown complete\n"
                << "  accepted blocks: "
                << accepted_blocks
                << '\n'
                << "  aggregate hashes checked: "
                << total_hashes
                << '\n';

            const int signal =
                static_cast<int>(
                    g_shutdown_signal);

            return signal != 0
                ? 128 + signal
                : 0;
        }

        std::cout
            << "Requested block count reached\n"
            << "  accepted blocks: "
            << accepted_blocks
            << '\n'
            << "  aggregate hashes checked: "
            << total_hashes
            << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "MercaMiner continuous mining failed: "
            << error.what()
            << '\n';

        return 1;
    }
}
