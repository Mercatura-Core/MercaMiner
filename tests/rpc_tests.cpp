// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <rpc.h>

#include <nlohmann/json.hpp>

#include <atomic>
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
    using mercaminer::ClassifySubmitBlockResult;
    using mercaminer::ParseRpcResponse;
    using mercaminer::RpcCallOptions;
    using mercaminer::RpcCancelledException;
    using mercaminer::RpcClient;
    using mercaminer::RpcCredentials;
    using mercaminer::RpcException;
    using mercaminer::SubmitBlockResult;

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

    std::string submit_rejection{"stale"};

    ok &= Check(
        ClassifySubmitBlockResult(
            nlohmann::json(nullptr),
            submit_rejection) ==
                SubmitBlockResult::NEEDS_TIP_CONFIRMATION &&
            submit_rejection.empty(),
        "submitblock null result requires tip confirmation");

    ok &= Check(
        ClassifySubmitBlockResult(
            nlohmann::json("duplicate"),
            submit_rejection) ==
                SubmitBlockResult::NEEDS_TIP_CONFIRMATION &&
            submit_rejection == "duplicate",
        "submitblock duplicate requires tip confirmation");

    ok &= Check(
        ClassifySubmitBlockResult(
            nlohmann::json("inconclusive"),
            submit_rejection) ==
                SubmitBlockResult::NEEDS_TIP_CONFIRMATION &&
            submit_rejection == "inconclusive",
        "submitblock inconclusive requires tip confirmation");

    ok &= Check(
        ClassifySubmitBlockResult(
            nlohmann::json("high-hash"),
            submit_rejection) ==
                SubmitBlockResult::REJECTED &&
            submit_rejection == "high-hash",
        "submitblock validation failure remains rejected");

    bool submit_nonstring_ok{false};

    try {
        (void)ClassifySubmitBlockResult(
            nlohmann::json(1),
            submit_rejection);
    } catch (const std::runtime_error& error) {
        submit_nonstring_ok =
            std::string{error.what()} ==
                "submitblock returned unexpected non-null result";
    }

    ok &= Check(
        submit_nonstring_ok,
        "unexpected submitblock result fails closed");

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

    const RpcCallOptions default_options{};

    ok &= Check(
        default_options.timeout_seconds == 30 &&
            default_options.cancelled == nullptr,
        "default RPC call options preserve 30-second timeout");

    RpcClient local_client{
        "http://127.0.0.1:1",
        RpcCredentials::FromUserPassword(
            "test",
            "test")};

    bool negative_timeout_ok{false};

    try {
        RpcCallOptions options;
        options.timeout_seconds = -1;

        (void)local_client.Call(
            "getblockcount",
            nlohmann::json::array(),
            options);
    } catch (const RpcException& error) {
        negative_timeout_ok =
            std::string{error.what()} ==
                "RPC timeout must not be negative";
    }

    ok &= Check(
        negative_timeout_ok,
        "negative RPC timeout rejected before transport");

    std::atomic_bool cancelled{true};

    bool preset_cancel_ok{false};

    try {
        RpcCallOptions options;
        options.timeout_seconds = 0;
        options.cancelled = &cancelled;

        (void)local_client.Call(
            "getblocktemplate",
            nlohmann::json::array(),
            options);
    } catch (const RpcCancelledException& error) {
        preset_cancel_ok =
            std::string{error.what()} ==
                "RPC request cancelled";
    }

    ok &= Check(
        preset_cancel_ok,
        "pre-cancelled RPC request aborts before transport");

    return ok ? 0 : 1;
}
