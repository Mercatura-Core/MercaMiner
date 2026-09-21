// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <rpc.h>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace mercaminer {
namespace {

class CurlGlobal
{
public:
    CurlGlobal()
    {
        const CURLcode result =
            curl_global_init(CURL_GLOBAL_DEFAULT);

        if (result != CURLE_OK) {
            throw RpcException(
                std::string{"curl_global_init failed: "} +
                curl_easy_strerror(result));
        }
    }

    ~CurlGlobal()
    {
        curl_global_cleanup();
    }

    CurlGlobal(const CurlGlobal&) = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;
};

void EnsureCurlInitialized()
{
    static CurlGlobal global;
    (void)global;
}

struct CurlDeleter
{
    void operator()(CURL* handle) const noexcept
    {
        if (handle != nullptr) {
            curl_easy_cleanup(handle);
        }
    }
};

struct CurlSlistDeleter
{
    void operator()(curl_slist* list) const noexcept
    {
        if (list != nullptr) {
            curl_slist_free_all(list);
        }
    }
};

std::size_t WriteResponse(
    char* data,
    std::size_t size,
    std::size_t count,
    void* context)
{
    if (count != 0 &&
        size > std::numeric_limits<std::size_t>::max() / count) {
        return 0;
    }

    const std::size_t bytes = size * count;

    auto& output =
        *static_cast<std::string*>(context);

    output.append(data, bytes);
    return bytes;
}

std::string TrimLineEnding(std::string value)
{
    while (!value.empty() &&
           (value.back() == '\n' ||
            value.back() == '\r')) {
        value.pop_back();
    }

    return value;
}

} // namespace

RpcException::RpcException(
    std::string message,
    std::optional<int> code)
    : std::runtime_error(std::move(message)),
      m_code(code)
{
}

RpcCredentials RpcCredentials::FromUserPassword(
    std::string_view user,
    std::string_view password)
{
    if (user.empty()) {
        throw RpcException("RPC username must not be empty");
    }

    if (user.find(':') != std::string_view::npos) {
        throw RpcException(
            "RPC username must not contain ':'");
    }

    return RpcCredentials{
        std::string{user} + ":" + std::string{password}};
}

RpcCredentials RpcCredentials::FromCookieFile(
    std::string_view path)
{
    std::ifstream file{std::string{path}};

    if (!file) {
        throw RpcException(
            "Unable to open RPC cookie file: " +
            std::string{path});
    }

    std::string cookie;
    std::getline(file, cookie);
    cookie = TrimLineEnding(std::move(cookie));

    const std::size_t separator = cookie.find(':');

    if (separator == std::string::npos ||
        separator == 0 ||
        separator + 1 >= cookie.size()) {
        throw RpcException(
            "Malformed RPC cookie file: " +
            std::string{path});
    }

    return RpcCredentials{std::move(cookie)};
}

nlohmann::json BuildRpcRequest(
    std::uint64_t id,
    std::string_view method,
    const nlohmann::json& params)
{
    if (method.empty()) {
        throw RpcException(
            "JSON-RPC method must not be empty");
    }

    return nlohmann::json{
        {"jsonrpc", "1.0"},
        {"id", id},
        {"method", std::string{method}},
        {"params", params},
    };
}

nlohmann::json ParseRpcResponse(
    std::string_view body,
    long http_status)
{
    if (http_status == 401 || http_status == 403) {
        std::ostringstream message;
        message
            << "RPC authentication failed"
            << " (HTTP " << http_status << ")";

        throw RpcException(message.str());
    }

    if (body.empty()) {
        std::ostringstream message;
        message
            << "Empty RPC response body"
            << " (HTTP " << http_status << ")";

        throw RpcException(message.str());
    }

    nlohmann::json response;

    try {
        response =
            nlohmann::json::parse(
                body.begin(),
                body.end());
    } catch (const nlohmann::json::parse_error& error) {
        std::ostringstream message;
        message
            << "Invalid JSON-RPC response"
            << " (HTTP " << http_status << "): "
            << error.what();

        throw RpcException(message.str());
    }

    if (!response.is_object()) {
        throw RpcException(
            "JSON-RPC response is not an object");
    }

    const auto error_it = response.find("error");

    if (error_it != response.end() &&
        !error_it->is_null()) {
        std::optional<int> code;
        std::string message{"JSON-RPC error"};

        if (error_it->is_object()) {
            const auto code_it = error_it->find("code");
            const auto message_it =
                error_it->find("message");

            if (code_it != error_it->end() &&
                code_it->is_number_integer()) {
                code = code_it->get<int>();
            }

            if (message_it != error_it->end() &&
                message_it->is_string()) {
                message = message_it->get<std::string>();
            }
        }

        throw RpcException(
            std::move(message),
            code);
    }

    if (http_status < 200 ||
        http_status >= 300) {
        std::ostringstream message;
        message
            << "RPC HTTP request failed with status "
            << http_status;

        throw RpcException(message.str());
    }

    const auto result_it = response.find("result");

    if (result_it == response.end()) {
        throw RpcException(
            "JSON-RPC response is missing result");
    }

    return *result_it;
}

RpcClient::RpcClient(
    std::string url,
    RpcCredentials credentials)
    : m_url(std::move(url)),
      m_credentials(std::move(credentials))
{
    if (m_url.empty()) {
        throw RpcException(
            "RPC URL must not be empty");
    }

    EnsureCurlInitialized();
}

nlohmann::json RpcClient::Call(
    std::string_view method,
    const nlohmann::json& params)
{
    const nlohmann::json request =
        BuildRpcRequest(
            ++m_next_id,
            method,
            params);

    const std::string request_body =
        request.dump();

    std::string response_body;

    std::unique_ptr<CURL, CurlDeleter> curl{
        curl_easy_init()};

    if (!curl) {
        throw RpcException(
            "curl_easy_init failed");
    }

    std::unique_ptr<curl_slist, CurlSlistDeleter>
        headers{
            curl_slist_append(
                nullptr,
                "Content-Type: application/json")};

    if (!headers) {
        throw RpcException(
            "Unable to allocate HTTP headers");
    }

    auto SetOption =
        [&](CURLoption option, auto value) {
            const CURLcode result =
                curl_easy_setopt(
                    curl.get(),
                    option,
                    value);

            if (result != CURLE_OK) {
                throw RpcException(
                    std::string{
                        "curl_easy_setopt failed: "} +
                    curl_easy_strerror(result));
            }
        };

    SetOption(
        CURLOPT_URL,
        m_url.c_str());

    SetOption(
        CURLOPT_PROTOCOLS,
        static_cast<long>(
            CURLPROTO_HTTP | CURLPROTO_HTTPS));

    SetOption(
        CURLOPT_POST,
        1L);

    SetOption(
        CURLOPT_HTTPHEADER,
        headers.get());

    SetOption(
        CURLOPT_POSTFIELDS,
        request_body.c_str());

    SetOption(
        CURLOPT_POSTFIELDSIZE_LARGE,
        static_cast<curl_off_t>(
            request_body.size()));

    SetOption(
        CURLOPT_USERPWD,
        m_credentials.basic_auth.c_str());

    SetOption(
        CURLOPT_HTTPAUTH,
        static_cast<long>(CURLAUTH_BASIC));

    SetOption(
        CURLOPT_WRITEFUNCTION,
        &WriteResponse);

    SetOption(
        CURLOPT_WRITEDATA,
        &response_body);

    SetOption(
        CURLOPT_CONNECTTIMEOUT,
        10L);

    SetOption(
        CURLOPT_TIMEOUT,
        30L);

    SetOption(
        CURLOPT_NOSIGNAL,
        1L);

    SetOption(
        CURLOPT_ACCEPT_ENCODING,
        "");

    const CURLcode result =
        curl_easy_perform(curl.get());

    if (result != CURLE_OK) {
        throw RpcException(
            std::string{"RPC transport error: "} +
            curl_easy_strerror(result));
    }

    long http_status{0};

    const CURLcode info_result =
        curl_easy_getinfo(
            curl.get(),
            CURLINFO_RESPONSE_CODE,
            &http_status);

    if (info_result != CURLE_OK) {
        throw RpcException(
            std::string{
                "Unable to read RPC HTTP status: "} +
            curl_easy_strerror(info_result));
    }

    return ParseRpcResponse(
        response_body,
        http_status);
}

} // namespace mercaminer
