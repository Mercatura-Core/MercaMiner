// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <network.h>
#include <rpc_connection.h>

#include <iostream>

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
bool ThrowsConnection(Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::RpcConnectionException&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    using mercaminer::FindNetworkIdentity;
    using mercaminer::LocalRpcConnectionForHome;

    bool ok{true};

    const auto* mainnet = FindNetworkIdentity("main");
    const auto* testnet = FindNetworkIdentity("test");
    const auto* signet = FindNetworkIdentity("signet");
    const auto* regtest = FindNetworkIdentity("regtest");

    if (!mainnet || !testnet || !signet || !regtest) {
        std::cerr << "FAIL network identities unavailable\n";
        return 1;
    }

    const auto main =
        LocalRpcConnectionForHome(
            *mainnet,
            "/home/tester");

    ok &= Check(
        main.rpc_url == "http://127.0.0.1:27776" &&
            main.cookie_file ==
                "/home/tester/.mercatura/.cookie",
        "mainnet local RPC defaults");

    const auto test =
        LocalRpcConnectionForHome(
            *testnet,
            "/home/tester");

    ok &= Check(
        test.rpc_url == "http://127.0.0.1:27775" &&
            test.cookie_file ==
                "/home/tester/.mercatura/testnet/.cookie",
        "testnet local RPC defaults");

    const auto signet_settings =
        LocalRpcConnectionForHome(
            *signet,
            "/home/tester");

    ok &= Check(
        signet_settings.rpc_url ==
                "http://127.0.0.1:27774" &&
            signet_settings.cookie_file ==
                "/home/tester/.mercatura/signet/.cookie",
        "signet local RPC defaults");

    const auto regtest_settings =
        LocalRpcConnectionForHome(
            *regtest,
            "/home/tester");

    ok &= Check(
        regtest_settings.rpc_url ==
                "http://127.0.0.1:27773" &&
            regtest_settings.cookie_file ==
                "/home/tester/.mercatura/regtest/.cookie",
        "regtest local RPC defaults");

    ok &= Check(
        ThrowsConnection([&] {
            LocalRpcConnectionForHome(
                *regtest,
                "");
        }),
        "empty HOME rejected");

    return ok ? 0 : 1;
}
