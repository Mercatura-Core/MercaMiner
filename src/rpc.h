// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_RPC_H
#define MERCAMINER_RPC_H

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mercaminer {

class RpcException : public std::runtime_error
{
public:
    explicit RpcException(
        std::string message,
        std::optional<int> code = std::nullopt);

    const std::optional<int>& code() const noexcept
    {
        return m_code;
    }

private:
    std::optional<int> m_code;
};

struct RpcCredentials
{
    std::string basic_auth;

    static RpcCredentials FromUserPassword(
        std::string_view user,
        std::string_view password);

    static RpcCredentials FromCookieFile(
        std::string_view path);
};

nlohmann::json BuildRpcRequest(
    std::uint64_t id,
    std::string_view method,
    const nlohmann::json& params);

nlohmann::json ParseRpcResponse(
    std::string_view body,
    long http_status);

class RpcClient
{
public:
    RpcClient(
        std::string url,
        RpcCredentials credentials);

    nlohmann::json Call(
        std::string_view method,
        const nlohmann::json& params);

private:
    std::string m_url;
    RpcCredentials m_credentials;
    std::uint64_t m_next_id{0};
};

} // namespace mercaminer

#endif // MERCAMINER_RPC_H
