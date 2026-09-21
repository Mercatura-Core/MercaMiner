// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <rpc.h>

#include <nlohmann/json.hpp>

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
    using mercaminer::BuildRpcRequest;
    using mercaminer::ParseRpcResponse;
    using mercaminer::RpcCredentials;
    using mercaminer::RpcException;

    bool ok{true};

    const RpcCredentials direct =
        RpcCredentials::FromUserPassword(
            "alice",
            "secret");

    ok &= Check(
        direct.basic_auth == "alice:secret",
        "user/password credentials constructed");

    ok &= Check(
        ThrowsRpc([] {
            RpcCredentials::FromUserPassword(
                "",
                "secret");
        }),
        "empty RPC username rejected");

    ok &= Check(
        ThrowsRpc([] {
            RpcCredentials::FromUserPassword(
                "bad:user",
                "secret");
        }),
        "colon in RPC username rejected");

    const std::filesystem::path cookie_path =
        std::filesystem::temp_directory_path() /
        "mercaminer_rpc_test.cookie";

    {
        std::ofstream cookie{cookie_path};
        cookie << "__cookie__:test-token\n";
    }

    const RpcCredentials cookie =
        RpcCredentials::FromCookieFile(
            cookie_path.string());

    ok &= Check(
        cookie.basic_auth ==
            "__cookie__:test-token",
        "cookie credentials loaded");

    std::filesystem::remove(cookie_path);

    const nlohmann::json request =
        BuildRpcRequest(
            7,
            "getblockchaininfo",
            nlohmann::json::array());

    ok &= Check(
        request.at("jsonrpc") == "1.0" &&
            request.at("id") == 7 &&
            request.at("method") ==
                "getblockchaininfo" &&
            request.at("params").is_array() &&
            request.at("params").empty(),
        "JSON-RPC request constructed");

    ok &= Check(
        ThrowsRpc([] {
            BuildRpcRequest(
                1,
                "",
                nlohmann::json::array());
        }),
        "empty JSON-RPC method rejected");

    const nlohmann::json result =
        ParseRpcResponse(
            R"({"result":{"chain":"regtest"},"error":null,"id":7})",
            200);

    ok &= Check(
        result.at("chain") == "regtest",
        "successful JSON-RPC result parsed");

    bool rpc_error_ok{false};

    try {
        (void)ParseRpcResponse(
            R"({"result":null,"error":{"code":-28,"message":"Loading block index..."},"id":1})",
            500);
    } catch (const RpcException& error) {
        rpc_error_ok =
            error.code().has_value() &&
            *error.code() == -28 &&
            std::string{error.what()} ==
                "Loading block index...";
    }

    ok &= Check(
        rpc_error_ok,
        "JSON-RPC error code and message preserved");

    bool authentication_error_ok{false};

    try {
        (void)ParseRpcResponse(
            "Unauthorized",
            401);
    } catch (const RpcException& error) {
        authentication_error_ok =
            !error.code().has_value() &&
            std::string{error.what()} ==
                "RPC authentication failed (HTTP 401)";
    }

    ok &= Check(
        authentication_error_ok,
        "RPC authentication failure reported clearly");

    ok &= Check(
        ThrowsRpc([] {
            (void)ParseRpcResponse(
                R"({"error":null,"id":1})",
                200);
        }),
        "missing JSON-RPC result rejected");

    ok &= Check(
        ThrowsRpc([] {
            (void)ParseRpcResponse(
                "",
                200);
        }),
        "empty RPC response rejected");

    ok &= Check(
        ThrowsRpc([] {
            (void)ParseRpcResponse(
                R"([])",
                200);
        }),
        "non-object JSON-RPC response rejected");

    return ok ? 0 : 1;
}
