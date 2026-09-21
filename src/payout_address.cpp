// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <payout_address.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mercaminer {
namespace {

unsigned char HexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return static_cast<unsigned char>(c - '0');
    }

    if (c >= 'a' && c <= 'f') {
        return static_cast<unsigned char>(c - 'a' + 10);
    }

    if (c >= 'A' && c <= 'F') {
        return static_cast<unsigned char>(c - 'A' + 10);
    }

    throw PayoutAddressException(
        "validateaddress returned invalid hexadecimal data");
}

Bytes ParseExactHex(
    const nlohmann::json& value,
    std::size_t expected_bytes,
    const char* field)
{
    if (!value.is_string()) {
        throw PayoutAddressException(
            std::string{"validateaddress field '"} +
            field +
            "' is not a string");
    }

    const std::string hex =
        value.get<std::string>();

    if (hex.size() != expected_bytes * 2) {
        throw PayoutAddressException(
            std::string{"validateaddress field '"} +
            field +
            "' has an unexpected length");
    }

    Bytes out;
    out.reserve(expected_bytes);

    for (std::size_t i = 0;
         i < hex.size();
         i += 2) {
        const unsigned char high =
            HexDigit(hex[i]);

        const unsigned char low =
            HexDigit(hex[i + 1]);

        out.push_back(
            static_cast<unsigned char>(
                (high << 4) | low));
    }

    return out;
}

} // namespace

Bytes ParsePayoutAddressResponse(
    const nlohmann::json& result)
{
    if (!result.is_object()) {
        throw PayoutAddressException(
            "validateaddress returned a non-object result");
    }

    const auto valid_it =
        result.find("isvalid");

    if (valid_it == result.end() ||
        !valid_it->is_boolean()) {
        throw PayoutAddressException(
            "validateaddress result is missing boolean isvalid");
    }

    if (!valid_it->get<bool>()) {
        std::string message{
            "payout address is invalid"};

        const auto error_it =
            result.find("error");

        if (error_it != result.end() &&
            error_it->is_string() &&
            !error_it->get_ref<
                const std::string&>().empty()) {
            message += ": ";
            message +=
                error_it->get_ref<
                    const std::string&>();
        }

        throw PayoutAddressException(message);
    }

    const auto witness_it =
        result.find("iswitness");

    if (witness_it == result.end() ||
        !witness_it->is_boolean() ||
        !witness_it->get<bool>()) {
        throw PayoutAddressException(
            "payout address is not a witness address");
    }

    const auto version_it =
        result.find("witness_version");

    if (version_it == result.end() ||
        (!version_it->is_number_integer() &&
         !version_it->is_number_unsigned()) ||
        version_it->get<std::int64_t>() != 2) {
        throw PayoutAddressException(
            "payout address is not Mercatura PQ witness v2");
    }

    const auto program_it =
        result.find("witness_program");

    if (program_it == result.end()) {
        throw PayoutAddressException(
            "validateaddress result is missing witness_program");
    }

    const Bytes program =
        ParseExactHex(
            *program_it,
            32,
            "witness_program");

    const auto script_it =
        result.find("scriptPubKey");

    if (script_it == result.end()) {
        throw PayoutAddressException(
            "validateaddress result is missing scriptPubKey");
    }

    const Bytes script =
        ParseExactHex(
            *script_it,
            34,
            "scriptPubKey");

    if (script[0] != 0x52 ||
        script[1] != 0x20) {
        throw PayoutAddressException(
            "Mercatura PQ payout script is not canonical witness v2");
    }

    if (!std::equal(
            program.begin(),
            program.end(),
            script.begin() + 2)) {
        throw PayoutAddressException(
            "validateaddress witness program does not match scriptPubKey");
    }

    return script;
}

Bytes ResolvePayoutAddress(
    RpcClient& rpc,
    std::string_view address)
{
    return ResolvePayoutAddress(
        rpc,
        address,
        RpcCallOptions{});
}

Bytes ResolvePayoutAddress(
    RpcClient& rpc,
    std::string_view address,
    const RpcCallOptions& options)
{
    if (address.empty()) {
        throw PayoutAddressException(
            "payout address must not be empty");
    }

    return ParsePayoutAddressResponse(
        rpc.Call(
            "validateaddress",
            nlohmann::json::array(
                {std::string{address}}),
            options));
}

} // namespace mercaminer
