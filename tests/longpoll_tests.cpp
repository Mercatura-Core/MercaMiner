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

    return ok ? 0 : 1;
}
