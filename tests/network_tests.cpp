// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <network.h>

#include <iostream>
#include <stdexcept>
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

mercaminer::UInt256 Parse256(const char* hex)
{
    const auto value =
        mercaminer::UInt256::FromHexBE(hex);

    if (!value) {
        throw std::runtime_error(
            "invalid test uint256 constant");
    }

    return *value;
}

template <typename Callable>
bool ThrowsNetwork(Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::NetworkException&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    using mercaminer::BlockchainInfo;
    using mercaminer::FindNetworkIdentity;
    using mercaminer::ValidateNetworkIdentity;

    bool ok{true};

    const auto* mainnet =
        FindNetworkIdentity("main");
    const auto* testnet =
        FindNetworkIdentity("test");
    const auto* signet =
        FindNetworkIdentity("signet");
    const auto* regtest =
        FindNetworkIdentity("regtest");

    ok &= Check(
        mainnet != nullptr &&
            mainnet->default_rpc_port == 27776 &&
            mainnet->genesis_hash ==
                "cd797c78731d68a82b664b3e359a2e69"
                "508ea873fe5747b686488589cc7d6f15",
        "mainnet identity pinned");

    ok &= Check(
        testnet != nullptr &&
            testnet->default_rpc_port == 27775 &&
            testnet->genesis_hash ==
                "0cee25abd571760687efbebbe8741873"
                "dc187ce46afe082282a47b2455320d73",
        "testnet identity pinned");

    ok &= Check(
        signet != nullptr &&
            signet->default_rpc_port == 27774 &&
            signet->genesis_hash ==
                "eebe2b23469b0d91056cc9240387ba7e"
                "e601ce138160da3c508142e039e1f36b",
        "signet identity pinned");

    ok &= Check(
        regtest != nullptr &&
            regtest->default_rpc_port == 27773 &&
            regtest->genesis_hash ==
                "8e2308efb3a16b126e69444329cc0ed8"
                "1bea0596e99db1032ccd750e7028f685",
        "regtest identity pinned");

    ok &= Check(
        FindNetworkIdentity("testnet4") == nullptr,
        "dormant testnet4 is not a supported miner network");

    ok &= Check(
        FindNetworkIdentity("unknown") == nullptr,
        "unknown network rejected");

    if (!regtest) {
        return 1;
    }

    BlockchainInfo blockchain;
    blockchain.chain = "regtest";

    const auto genesis =
        Parse256(
            "8e2308efb3a16b126e69444329cc0ed8"
            "1bea0596e99db1032ccd750e7028f685");

    bool valid_identity{true};

    try {
        ValidateNetworkIdentity(
            *regtest,
            blockchain,
            genesis);
    } catch (...) {
        valid_identity = false;
    }

    ok &= Check(
        valid_identity,
        "matching chain and genesis accepted");

    BlockchainInfo wrong_chain = blockchain;
    wrong_chain.chain = "main";

    ok &= Check(
        ThrowsNetwork([&] {
            ValidateNetworkIdentity(
                *regtest,
                wrong_chain,
                genesis);
        }),
        "chain-name mismatch rejected");

    const auto wrong_genesis =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000001");

    ok &= Check(
        ThrowsNetwork([&] {
            ValidateNetworkIdentity(
                *regtest,
                blockchain,
                wrong_genesis);
        }),
        "genesis mismatch rejected");

    return ok ? 0 : 1;
}
