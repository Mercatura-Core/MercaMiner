// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <candidate_miner.h>
#include <gbt.h>
#include <hash256.h>
#include <mining_job.h>
#include <network.h>
#include <rpc.h>
#include <scanner.h>
#include <serialization.h>
#include <uint256.h>

#include <nlohmann/json.hpp>

#include <charconv>
#include <chrono>
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

unsigned char HexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return static_cast<unsigned char>(c - '0');
    }

    if (c >= 'a' && c <= 'f') {
        return static_cast<unsigned char>(c - 'a' + 10);
    }

    if (c >= 'A' && c <= 'F') {
        return static_cast<unsigned char>(c - 'A' + 10);
    }

    throw std::runtime_error(
        "invalid hexadecimal character");
}

mercaminer::Bytes ParseHex(
    std::string_view hex)
{
    if ((hex.size() % 2) != 0) {
        throw std::runtime_error(
            "hex string has odd length");
    }

    mercaminer::Bytes out;
    out.reserve(hex.size() / 2);

    for (std::size_t i = 0;
         i < hex.size();
         i += 2) {
        const unsigned char high =
            HexDigit(hex[i]);

        const unsigned char low =
            HexDigit(hex[i + 1]);

        out.push_back(
            static_cast<unsigned char>(
                (high << 4) | low));
    }

    return out;
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
    mercaminer::RpcClient& rpc)
{
    CurrentWork work;

    work.blockchain =
        mercaminer::ParseBlockchainInfo(
            rpc.Call(
                "getblockchaininfo",
                nlohmann::json::array()));

    work.block_template =
        mercaminer::ParseBlockTemplate(
            rpc.Call(
                "getblocktemplate",
                nlohmann::json::array(
                    {TemplateRequest()})));

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
};

SubmitOutcome SubmitCandidate(
    mercaminer::RpcClient& rpc,
    const mercaminer::BlockCandidate& candidate,
    std::string& rejection)
{
    const auto result =
        rpc.Call(
            "submitblock",
            nlohmann::json::array(
                {Hex(candidate.serialized_block)}));

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
    const mercaminer::UInt256& expected_hash)
{
    const std::uint64_t height =
        ParseRpcHeight(
            rpc.Call(
                "getblockcount",
                nlohmann::json::array()),
            "getblockcount");

    const auto best =
        ParseRpcHash(
            rpc.Call(
                "getbestblockhash",
                nlohmann::json::array()),
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
    std::string& rejection)
{
    for (;;) {
        try {
            rejection.clear();

            const SubmitOutcome outcome =
                SubmitCandidate(
                    rpc,
                    candidate,
                    rejection);

            if (outcome ==
                SubmitOutcome::REJECTED) {
                return
                    ResolvedSubmitOutcome::REJECTED;
            }

            if (ConfirmAcceptedTip(
                    rpc,
                    expected_height,
                    expected_hash)) {
                return
                    ResolvedSubmitOutcome::ACCEPTED;
            }

            return
                ResolvedSubmitOutcome::NOT_CURRENT_TIP;
        } catch (const mercaminer::RpcException& error) {
            std::cerr
                << "RPC error while submitting or confirming "
                << "solved candidate: "
                << error.what()
                << '\n'
                << "Retrying the same solved candidate "
                << "in 1 second\n";

            std::this_thread::sleep_for(
                std::chrono::seconds{1});
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    using mercaminer::FindNetworkIdentity;
    using mercaminer::MineBlockCandidate;
    using mercaminer::MiningJob;
    using mercaminer::NonceScanner;
    using mercaminer::RpcClient;
    using mercaminer::RpcCredentials;
    using mercaminer::RpcException;
    using mercaminer::ScanStatus;
    using mercaminer::ValidateNetworkIdentity;

    if (argc != 6) {
        std::cerr
            << "Usage: "
            << argv[0]
            << " <network> <rpc-url> <cookie-file>"
            << " <payout-script-hex> <block-count>\n"
            << "block-count 0 means run continuously\n";

        return 2;
    }

    const std::string network_name{argv[1]};
    const std::string rpc_url{argv[2]};
    const std::string cookie_file{argv[3]};
    const std::string payout_hex{argv[4]};

    if (network_name != "regtest") {
        std::cerr
            << "This M7 continuous miner is restricted "
            << "to regtest.\n";

        return 2;
    }

    try {
        const std::uint64_t block_limit =
            ParseBlockLimit(argv[5]);

        const auto payout_script =
            ParseHex(payout_hex);

        if (payout_script.empty()) {
            throw std::runtime_error(
                "payout script must not be empty");
        }

        const auto* network =
            FindNetworkIdentity(
                network_name);

        if (network == nullptr) {
            throw std::runtime_error(
                "unsupported Mercatura network");
        }

        RpcClient rpc{
            rpc_url,
            RpcCredentials::FromCookieFile(
                cookie_file)};

        const auto initial_blockchain =
            mercaminer::ParseBlockchainInfo(
                rpc.Call(
                    "getblockchaininfo",
                    nlohmann::json::array()));

        const auto live_genesis =
            ParseRpcHash(
                rpc.Call(
                    "getblockhash",
                    nlohmann::json::array({0})),
                "getblockhash 0");

        ValidateNetworkIdentity(
            *network,
            initial_blockchain,
            live_genesis);

        NonceScanner scanner;

        std::uint64_t accepted_blocks{0};
        std::uint64_t total_hashes{0};

        std::cout
            << "MercaMiner continuous regtest mining started\n";

        if (block_limit == 0) {
            std::cout
                << "  block limit: continuous\n";
        } else {
            std::cout
                << "  block limit: "
                << block_limit
                << '\n';
        }

        while (block_limit == 0 ||
               accepted_blocks < block_limit) {
            try {
                CurrentWork work =
                    FetchCurrentWork(rpc);

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

                bool refresh_template{false};

                while (job.HasMoreCandidates() &&
                       !refresh_template) {
                    auto mining_candidate =
                        job.NextCandidate();

                    std::cout
                        << "Mining height "
                        << work.block_template.height
                        << " extranonce "
                        << mining_candidate.extranonce
                        << '\n';

                    const auto result =
                        MineBlockCandidate(
                            scanner,
                            mining_candidate.candidate,
                            work.block_template.target,
                            work.block_template.nonce_min,
                            work.block_template.nonce_max);

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
                        throw std::runtime_error(
                            "mining unexpectedly cancelled");
                    }

                    if (result.status ==
                        ScanStatus::EXHAUSTED) {
                        std::cout
                            << "Nonce range exhausted after "
                            << result.hashes_checked
                            << " hashes; advancing extranonce\n";

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
                            rejection);

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
                        << "  block hashes checked: "
                        << result.hashes_checked
                        << '\n'
                        << "  session hashes checked: "
                        << total_hashes
                        << '\n'
                        << "  accepted this session: "
                        << accepted_blocks
                        << '\n';

                    refresh_template = true;
                }

                if (!job.HasMoreCandidates() &&
                    !refresh_template) {
                    throw std::runtime_error(
                        "64-bit extranonce space exhausted");
                }
            } catch (const RpcException& error) {
                std::cerr
                    << "RPC error: "
                    << error.what()
                    << "\nRetrying in 1 second\n";

                std::this_thread::sleep_for(
                    std::chrono::seconds{1});
            }
        }

        std::cout
            << "Requested block count reached\n"
            << "  accepted blocks: "
            << accepted_blocks
            << '\n'
            << "  total hashes checked: "
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
