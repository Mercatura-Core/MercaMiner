// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <coinbase.h>

#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string Hex(
    std::span<const unsigned char> bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (const unsigned char byte : bytes) {
        out << std::setw(2)
            << static_cast<unsigned int>(byte);
    }

    return out.str();
}

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
bool ThrowsCoinbase(
    Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::CoinbaseException&) {
        return true;
    }

    return false;
}

mercaminer::Bytes Commitment()
{
    mercaminer::Bytes result{
        0x6a,
        0x24,
        0xaa,
        0x21,
        0xa9,
        0xed};

    result.insert(
        result.end(),
        32,
        0x11);

    return result;
}

mercaminer::Bytes PayoutScript()
{
    mercaminer::Bytes result{
        0x52,
        0x20};

    for (unsigned int i = 0;
         i < 32;
         ++i) {
        result.push_back(
            static_cast<unsigned char>(i));
    }

    return result;
}

} // namespace

int main()
{
    using mercaminer::BuildBip34HeightPrefix;
    using mercaminer::BuildCoinbaseTransaction;
    using mercaminer::Bytes;

    bool ok{true};

    struct HeightVector
    {
        std::uint32_t height;
        const char* expected;
    };

    static constexpr HeightVector height_vectors[]{
        {1, "51"},
        {2, "52"},
        {16, "60"},
        {17, "0111"},
        {127, "017f"},
        {128, "028000"},
        {255, "02ff00"},
        {256, "020001"},
    };

    for (const auto& vector : height_vectors) {
        ok &= Check(
            Hex(
                BuildBip34HeightPrefix(
                    vector.height)) ==
                vector.expected,
            "BIP34 height vector");
    }

    ok &= Check(
        ThrowsCoinbase([] {
            BuildBip34HeightPrefix(0);
        }),
        "zero height rejected");

    const Bytes payout =
        PayoutScript();

    const Bytes commitment =
        Commitment();

    const Bytes empty;

    const Bytes extranonce{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08};

    const auto coinbase =
        BuildCoinbaseTransaction(
            1,
            2378234,
            payout,
            empty,
            extranonce,
            commitment);

    ok &= Check(
        Hex(coinbase.script_sig) ==
            "510102030405060708",
        "height and extranonce scriptSig");

    ok &= Check(
        coinbase.serialized_without_witness.size() ==
            150,
        "stripped coinbase size");

    ok &= Check(
        coinbase.serialized_with_witness.size() ==
            186,
        "witness coinbase size");

    ok &= Check(
        Hex(
            coinbase.serialized_without_witness) ==
            "02000000"
            "01"
            "00000000000000000000000000000000"
            "00000000000000000000000000000000"
            "ffffffff"
            "09"
            "510102030405060708"
            "feffffff"
            "02"
            "fa49240000000000"
            "22"
            "5220000102030405060708090a0b0c0d"
            "0e0f101112131415161718191a1b1c1d"
            "1e1f"
            "0000000000000000"
            "26"
            "6a24aa21a9ed"
            "11111111111111111111111111111111"
            "11111111111111111111111111111111"
            "00000000",
        "stripped coinbase serialization");

    ok &= Check(
        Hex(
            coinbase.serialized_with_witness) ==
            "02000000"
            "0001"
            "01"
            "00000000000000000000000000000000"
            "00000000000000000000000000000000"
            "ffffffff"
            "09"
            "510102030405060708"
            "feffffff"
            "02"
            "fa49240000000000"
            "22"
            "5220000102030405060708090a0b0c0d"
            "0e0f101112131415161718191a1b1c1d"
            "1e1f"
            "0000000000000000"
            "26"
            "6a24aa21a9ed"
            "11111111111111111111111111111111"
            "11111111111111111111111111111111"
            "01"
            "20"
            "00000000000000000000000000000000"
            "00000000000000000000000000000000"
            "00000000",
        "witness coinbase serialization");

    ok &= Check(
        coinbase.txid.ToHexBE() ==
            "99936781e478dbe6578bc08c03f6e479"
            "b11529084d44d8bae5c4bd408c6f3f86",
        "coinbase txid vector");

    ok &= Check(
        coinbase.wtxid.ToHexBE() ==
            "0a2283d160f0766a9d158cb26dfb5d4b"
            "117d775a7b5199aa992c890a48fafeed",
        "coinbase wtxid vector");

    ok &= Check(
        coinbase.txid != coinbase.wtxid,
        "coinbase txid differs from wtxid");

    const Bytes aux_flags{
        0xaa,
        0xbb};

    const Bytes extra_two{
        0xcc,
        0xdd};

    const auto aux_coinbase =
        BuildCoinbaseTransaction(
            17,
            1,
            payout,
            aux_flags,
            extra_two,
            commitment);

    ok &= Check(
        Hex(aux_coinbase.script_sig) ==
            "0111aabbccdd",
        "coinbaseaux precedes extranonce");

    ok &= Check(
        ThrowsCoinbase([&] {
            BuildCoinbaseTransaction(
                1,
                1,
                payout,
                empty,
                empty,
                commitment);
        }),
        "too-short coinbase scriptSig rejected");

    const Bytes max_extra(
        98,
        0x42);

    const auto max_coinbase =
        BuildCoinbaseTransaction(
            17,
            1,
            payout,
            empty,
            max_extra,
            commitment);

    ok &= Check(
        max_coinbase.script_sig.size() == 100,
        "100-byte coinbase scriptSig accepted");

    const Bytes too_large_extra(
        99,
        0x42);

    ok &= Check(
        ThrowsCoinbase([&] {
            BuildCoinbaseTransaction(
                17,
                1,
                payout,
                empty,
                too_large_extra,
                commitment);
        }),
        "101-byte coinbase scriptSig rejected");

    ok &= Check(
        ThrowsCoinbase([&] {
            BuildCoinbaseTransaction(
                1,
                -1,
                payout,
                empty,
                extranonce,
                commitment);
        }),
        "negative coinbase value rejected");

    Bytes bad_commitment =
        commitment;
    bad_commitment[0] = 0x00;

    ok &= Check(
        ThrowsCoinbase([&] {
            BuildCoinbaseTransaction(
                1,
                1,
                payout,
                empty,
                extranonce,
                bad_commitment);
        }),
        "bad witness commitment prefix rejected");

    Bytes short_commitment(
        commitment.begin(),
        commitment.end() - 1);

    ok &= Check(
        ThrowsCoinbase([&] {
            BuildCoinbaseTransaction(
                1,
                1,
                payout,
                empty,
                extranonce,
                short_commitment);
        }),
        "bad witness commitment length rejected");

    return ok ? 0 : 1;
}
