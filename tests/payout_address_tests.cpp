// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <payout_address.h>

#include <nlohmann/json.hpp>

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
bool ThrowsPayout(Callable&& callable)
{
    try {
        callable();
    } catch (
        const mercaminer::PayoutAddressException&) {
        return true;
    }

    return false;
}

nlohmann::json ValidResponse()
{
    const std::string program =
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f";

    return nlohmann::json{
        {"isvalid", true},
        {"address", "mcrt1z-test"},
        {"scriptPubKey", "5220" + program},
        {"isscript", true},
        {"iswitness", true},
        {"witness_version", 2},
        {"witness_program", program},
    };
}

} // namespace

int main()
{
    using mercaminer::ParsePayoutAddressResponse;

    bool ok{true};

    const auto valid =
        ParsePayoutAddressResponse(
            ValidResponse());

    ok &= Check(
        valid.size() == 34 &&
            valid[0] == 0x52 &&
            valid[1] == 0x20 &&
            valid[2] == 0x00 &&
            valid[33] == 0x1f,
        "canonical Mercatura PQ payout accepted");

    ok &= Check(
        ThrowsPayout([] {
            (void)ParsePayoutAddressResponse(
                nlohmann::json::array());
        }),
        "non-object response rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value.erase("isvalid");
            (void)ParsePayoutAddressResponse(value);
        }),
        "missing isvalid rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value["isvalid"] = false;
            value["error"] = "Invalid Bech32m address";
            (void)ParsePayoutAddressResponse(value);
        }),
        "invalid address rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value["iswitness"] = false;
            (void)ParsePayoutAddressResponse(value);
        }),
        "non-witness address rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value["witness_version"] = 1;
            (void)ParsePayoutAddressResponse(value);
        }),
        "non-PQ witness version rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value["witness_program"] = "00";
            (void)ParsePayoutAddressResponse(value);
        }),
        "wrong-sized PQ witness program rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value["scriptPubKey"] =
                "5120"
                "000102030405060708090a0b0c0d0e0f"
                "101112131415161718191a1b1c1d1e1f";
            (void)ParsePayoutAddressResponse(value);
        }),
        "non-v2 payout script rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value["scriptPubKey"] =
                "5220"
                "ff0102030405060708090a0b0c0d0e0f"
                "101112131415161718191a1b1c1d1e1f";
            (void)ParsePayoutAddressResponse(value);
        }),
        "script and witness-program mismatch rejected");

    ok &= Check(
        ThrowsPayout([] {
            auto value = ValidResponse();
            value["scriptPubKey"] =
                "5220"
                "zz0102030405060708090a0b0c0d0e0f"
                "101112131415161718191a1b1c1d1e1f";
            (void)ParsePayoutAddressResponse(value);
        }),
        "non-hexadecimal payout script rejected");

    return ok ? 0 : 1;
}
