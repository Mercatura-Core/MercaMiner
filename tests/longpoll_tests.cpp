// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <longpoll.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

bool Check(
    bool condition,
    const char* name)
{
    if (!condition) {
        std::cerr
            << "FAIL "
            << name
            << '\n';

        return false;
    }

    std::cout
        << "PASS "
        << name
        << '\n';

    return true;
}

mercaminer::UInt256 Parse256(
    const char* hex)
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
bool ThrowsInvalidArgument(
    Callable&& callable)
{
    try {
        callable();
    } catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    using mercaminer::BuildLongpollTemplateRequest;
    using mercaminer::LongpollTemplateChangesTip;
    using mercaminer::LongpollTemplateInvalidatesOldWork;

    bool ok{true};

    const auto request =
        BuildLongpollTemplateRequest(
            "tip-hash-and-sequence");

    ok &= Check(
        request.is_object() &&
            request.at("rules") ==
                nlohmann::json::array(
                    {"segwit"}) &&
            request.at("longpollid") ==
                "tip-hash-and-sequence",
        "longpoll GBT request constructed");

    ok &= Check(
        ThrowsInvalidArgument([] {
            (void)BuildLongpollTemplateRequest("");
        }),
        "empty longpoll id rejected");

    const auto original_tip =
        Parse256(
            "11111111111111111111111111111111"
            "11111111111111111111111111111111");

    const auto replacement_tip =
        Parse256(
            "22222222222222222222222222222222"
            "22222222222222222222222222222222");

    ok &= Check(
        !LongpollTemplateChangesTip(
            original_tip,
            original_tip),
        "same-tip template refresh is not stale work");

    ok &= Check(
        LongpollTemplateChangesTip(
            original_tip,
            replacement_tip),
        "changed template parent is stale work");

    ok &= Check(
        !LongpollTemplateInvalidatesOldWork(
            original_tip,
            original_tip,
            true),
        "same-tip submitold=true preserves old work");

    ok &= Check(
        LongpollTemplateInvalidatesOldWork(
            original_tip,
            original_tip,
            false),
        "same-tip submitold=false invalidates old work");

    ok &= Check(
        LongpollTemplateInvalidatesOldWork(
            original_tip,
            replacement_tip,
            true),
        "new parent invalidates old work");

    return ok ? 0 : 1;
}
