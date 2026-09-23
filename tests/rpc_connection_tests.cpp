// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <network.h>
#include <rpc_connection.h>

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
bool ThrowsConnection(Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::RpcConnectionException&) {
        return true;
    }

    return false;
}

template <typename Callable>
bool ThrowsRpc(Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::RpcException&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    using mercaminer::FindNetworkIdentity;
    using mercaminer::LocalRpcConnectionForHome;
    using mercaminer::ReloadRpcClientFromCookie;
    using mercaminer::RpcClient;
    using mercaminer::RpcConnectionSettings;
    using mercaminer::RpcCredentials;

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

    const std::filesystem::path cookie_path =
        std::filesystem::temp_directory_path() /
        "mercaminer_rpc_connection_test.cookie";

    std::filesystem::remove(cookie_path);

    {
        std::ofstream cookie{cookie_path};
        cookie << "__cookie__:token-a\n";
    }

    const RpcConnectionSettings refresh_connection{
        "http://127.0.0.1:27773",
        cookie_path.string()};

    RpcCredentials refresh_credentials =
        RpcCredentials::FromCookieFile(
            cookie_path.string());

    RpcClient refresh_rpc{
        refresh_connection.rpc_url,
        refresh_credentials};

    {
        std::ofstream cookie{
            cookie_path,
            std::ios::trunc};
        cookie << "__cookie__:token-b\n";
    }

    const bool changed =
        ReloadRpcClientFromCookie(
            refresh_connection,
            refresh_credentials,
            refresh_rpc);

    ok &= Check(
        changed &&
            refresh_credentials.basic_auth ==
                "__cookie__:token-b",
        "rotated RPC cookie reloaded");

    const bool unchanged =
        ReloadRpcClientFromCookie(
            refresh_connection,
            refresh_credentials,
            refresh_rpc);

    ok &= Check(
        !unchanged &&
            refresh_credentials.basic_auth ==
                "__cookie__:token-b",
        "unchanged RPC cookie not replaced");

    std::filesystem::remove(cookie_path);

    ok &= Check(
        ThrowsRpc([&] {
            (void)ReloadRpcClientFromCookie(
                refresh_connection,
                refresh_credentials,
                refresh_rpc);
        }) &&
            refresh_credentials.basic_auth ==
                "__cookie__:token-b",
        "failed RPC cookie reload preserves credentials");

    return ok ? 0 : 1;
}
