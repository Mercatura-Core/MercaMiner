// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <gbt.h>
#include <network.h>
#include <rpc.h>
#include <uint256.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

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

} // namespace

int main(int argc, char* argv[])
{
    using mercaminer::BlockTemplate;
    using mercaminer::BlockchainInfo;
    using mercaminer::FindNetworkIdentity;
    using mercaminer::ParseBlockchainInfo;
    using mercaminer::ParseBlockTemplate;
    using mercaminer::RpcClient;
    using mercaminer::RpcCredentials;
    using mercaminer::ValidateNetworkIdentity;

    if (argc != 4) {
        std::cerr
            << "Usage: " << argv[0]
            << " <network> <rpc-url> <cookie-file>\n";

        return 2;
    }

    const std::string network_name{argv[1]};
    const std::string rpc_url{argv[2]};
    const std::string cookie_file{argv[3]};

    const auto* network =
        FindNetworkIdentity(network_name);

    if (network == nullptr) {
        std::cerr
            << "Unsupported Mercatura network: "
            << network_name << '\n';

        return 2;
    }

    try {
        RpcClient rpc{
            rpc_url,
            RpcCredentials::FromCookieFile(
                cookie_file)};

        const BlockchainInfo blockchain =
            ParseBlockchainInfo(
                rpc.Call(
                    "getblockchaininfo",
                    nlohmann::json::array()));

        const auto genesis_json =
            rpc.Call(
                "getblockhash",
                nlohmann::json::array({0}));

        const auto live_genesis =
            ParseRpcHash(
                genesis_json,
                "getblockhash 0");

        ValidateNetworkIdentity(
            *network,
            blockchain,
            live_genesis);

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
                "getblocktemplate previousblockhash "
                "does not match current best block");
        }

        std::cout
            << "Mercatura node/template validation passed\n"
            << "  network: " << network->chain << '\n'
            << "  RPC port: "
            << network->default_rpc_port << '\n'
            << "  genesis: "
            << live_genesis.ToHexBE() << '\n'
            << "  current height: "
            << blockchain.blocks << '\n'
            << "  template height: "
            << block_template.height << '\n'
            << "  previous block: "
            << block_template.previous_block_hash.ToHexBE()
            << '\n'
            << "  bits: "
            << std::hex
            << std::setfill('0')
            << std::setw(8)
            << block_template.bits
            << std::dec << '\n'
            << "  target: "
            << block_template.target.ToHexBE() << '\n'
            << "  coinbase value: "
            << block_template.coinbase_value
            << " base units\n"
            << "  transactions: "
            << block_template.transactions.size()
            << '\n'
            << "  nonce range: "
            << std::hex
            << std::setfill('0')
            << std::setw(8)
            << block_template.nonce_min
            << '-'
            << std::setw(8)
            << block_template.nonce_max
            << std::dec << '\n'
            << "  serialized size limit: "
            << block_template.size_limit
            << " bytes\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "MercaMiner node validation failed: "
            << error.what() << '\n';

        return 1;
    }
}
