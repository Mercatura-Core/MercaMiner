// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <block_builder.h>
#include <candidate_miner.h>
#include <gbt.h>
#include <hash256.h>
#include <mining_job.h>
#include <network.h>
#include <payout_address.h>
#include <rpc.h>
#include <scanner.h>
#include <serialization.h>
#include <uint256.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

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

} // namespace

int main(int argc, char* argv[])
{
    using mercaminer::BlockTemplate;
    using mercaminer::BlockchainInfo;
    using mercaminer::BuildBlockCandidate;
    using mercaminer::FindNetworkIdentity;
    using mercaminer::MineBlockCandidate;
    using mercaminer::MiningJob;
    using mercaminer::NonceScanner;
    using mercaminer::ParseBlockchainInfo;
    using mercaminer::ParseBlockTemplate;
    using mercaminer::RpcClient;
    using mercaminer::RpcCredentials;
    using mercaminer::ScanStatus;
    using mercaminer::ValidateNetworkIdentity;

    if (argc != 5) {
        std::cerr
            << "Usage: "
            << argv[0]
            << " <network> <rpc-url> <cookie-file>"
            << " <payout-address>\n";

        return 2;
    }

    const std::string network_name{argv[1]};
    const std::string rpc_url{argv[2]};
    const std::string cookie_file{argv[3]};
    const std::string payout_address{argv[4]};

    if (network_name != "regtest") {
        std::cerr
            << "MercaMiner one-shot mining is currently "
            << "restricted to regtest.\n";

        return 2;
    }

    try {
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

        const BlockchainInfo blockchain =
            ParseBlockchainInfo(
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
            blockchain,
            live_genesis);

        const auto payout_script =
            mercaminer::ResolvePayoutAddress(
                rpc,
                payout_address);

        const nlohmann::json template_request{
            {
                "rules",
                nlohmann::json::array(
                    {"segwit"})
            }
        };

        const BlockTemplate block_template =
            ParseBlockTemplate(
                rpc.Call(
                    "getblocktemplate",
                    nlohmann::json::array(
                        {template_request})));

        if (block_template.previous_block_hash !=
            blockchain.best_block_hash) {
            throw std::runtime_error(
                "template previous block does not "
                "match current best block");
        }

        if (block_template.height !=
            blockchain.blocks + 1) {
            throw std::runtime_error(
                "template height is not current "
                "chain height plus one");
        }

        MiningJob job{
            block_template,
            payout_script,
            block_template.height};

        auto work =
            job.NextCandidate();

        auto candidate =
            std::move(work.candidate);

        NonceScanner scanner;

        std::cout
            << "Mining Mercatura regtest block\n"
            << "  height: "
            << block_template.height
            << '\n'
            << "  previous block: "
            << block_template.previous_block_hash.ToHexBE()
            << '\n'
            << "  target: "
            << block_template.target.ToHexBE()
            << '\n'
            << "  nonce range: "
            << block_template.nonce_min
            << '-'
            << block_template.nonce_max
            << '\n'
            << "  extranonce: "
            << work.extranonce
            << '\n'
            << "  candidate size: "
            << candidate.serialized_block.size()
            << " bytes\n";

        const auto result =
            MineBlockCandidate(
                scanner,
                candidate,
                block_template.target,
                block_template.nonce_min,
                block_template.nonce_max);

        if (result.status ==
            ScanStatus::CANCELLED) {
            throw std::runtime_error(
                "mining unexpectedly cancelled");
        }

        if (result.status ==
            ScanStatus::EXHAUSTED) {
            throw std::runtime_error(
                "nonce range exhausted without "
                "finding proof of work");
        }

        std::cout
            << "Proof of work found\n"
            << "  nonce: "
            << result.nonce
            << '\n'
            << "  MercaHash: "
            << result.hash.ToHexBE()
            << '\n'
            << "  hashes checked: "
            << result.hashes_checked
            << '\n';

        const auto solved_header =
            candidate.header.Serialize();

        const auto expected_block_hash =
            mercaminer::DoubleSha256(
                solved_header);

        const std::string block_hex =
            Hex(candidate.serialized_block);

        const auto submit_result =
            rpc.Call(
                "submitblock",
                nlohmann::json::array(
                    {block_hex}));

        if (!submit_result.is_null()) {
            if (submit_result.is_string()) {
                throw std::runtime_error(
                    "submitblock rejected block: " +
                    submit_result.get<std::string>());
            }

            throw std::runtime_error(
                "submitblock returned unexpected "
                "non-null result");
        }

        const std::uint64_t new_height =
            ParseRpcHeight(
                rpc.Call(
                    "getblockcount",
                    nlohmann::json::array()),
                "getblockcount");

        const auto new_best =
            ParseRpcHash(
                rpc.Call(
                    "getbestblockhash",
                    nlohmann::json::array()),
                "getbestblockhash");

        if (new_height !=
            blockchain.blocks + 1) {
            throw std::runtime_error(
                "block submission returned success "
                "but chain height did not advance "
                "by exactly one");
        }

        if (new_best !=
            expected_block_hash) {
            throw std::runtime_error(
                "accepted best-block hash does not "
                "match locally computed block ID");
        }

        std::cout
            << "submitblock accepted\n"
            << "  new height: "
            << new_height
            << '\n'
            << "  block hash: "
            << new_best.ToHexBE()
            << '\n'
            << "  locally computed block ID: "
            << expected_block_hash.ToHexBE()
            << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "MercaMiner mining failed: "
            << error.what()
            << '\n';

        return 1;
    }
}
