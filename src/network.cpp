// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <network.h>

#include <array>
#include <string>

namespace mercaminer {
namespace {

static constexpr std::array<NetworkIdentity, 4> NETWORKS{{
    {
        "main",
        27776,
        "",
        "cd797c78731d68a82b664b3e359a2e69"
        "508ea873fe5747b686488589cc7d6f15",
        "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    },
    {
        "test",
        27775,
        "testnet",
        "0cee25abd571760687efbebbe8741873"
        "dc187ce46afe082282a47b2455320d73",
        "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    },
    {
        "signet",
        27774,
        "signet",
        "eebe2b23469b0d91056cc9240387ba7e"
        "e601ce138160da3c508142e039e1f36b",
        "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    },
    {
        "regtest",
        27773,
        "regtest",
        "8e2308efb3a16b126e69444329cc0ed8"
        "1bea0596e99db1032ccd750e7028f685",
        "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    },
}};

} // namespace

const NetworkIdentity*
FindNetworkIdentity(std::string_view chain) noexcept
{
    for (const auto& network : NETWORKS) {
        if (network.chain == chain) {
            return &network;
        }
    }

    return nullptr;
}

bool SupportsDirectPowMining(
    const NetworkIdentity& network) noexcept
{
    return
        network.chain == "main" ||
        network.chain == "test" ||
        network.chain == "regtest";
}

void ValidateNetworkIdentity(
    const NetworkIdentity& expected,
    const BlockchainInfo& blockchain,
    const UInt256& live_genesis)
{
    if (blockchain.chain != expected.chain) {
        throw NetworkException(
            "RPC node reports chain '" +
            blockchain.chain +
            "' but MercaMiner expected '" +
            std::string{expected.chain} + "'");
    }

    const auto expected_genesis =
        UInt256::FromHexBE(expected.genesis_hash);

    if (!expected_genesis) {
        throw NetworkException(
            "Internal MercaMiner genesis constant is invalid");
    }

    if (live_genesis != *expected_genesis) {
        throw NetworkException(
            "RPC node genesis hash does not match "
            "the expected Mercatura " +
            std::string{expected.chain} +
            " genesis");
    }
}

void ValidateProofOfWorkTarget(
    const NetworkIdentity& network,
    const UInt256& target)
{
    const auto pow_limit =
        UInt256::FromHexBE(network.pow_limit);

    if (!pow_limit) {
        throw NetworkException(
            "Internal MercaMiner powLimit constant is invalid");
    }

    if (target > *pow_limit) {
        throw NetworkException(
            "proof-of-work target exceeds Mercatura " +
            std::string{network.chain} +
            " powLimit");
    }
}

} // namespace mercaminer
