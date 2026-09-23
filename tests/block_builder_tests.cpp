// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <block_builder.h>
#include <hash256.h>
#include <merkle.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>

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
bool ThrowsBuild(
    Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::BlockBuildException&) {
        return true;
    }

    return false;
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

mercaminer::BlockTemplate BaseTemplate()
{
    mercaminer::BlockTemplate block_template;

    block_template.version = 536870912;

    block_template.previous_block_hash =
        Parse256(
            "8e2308efb3a16b126e69444329cc0ed8"
            "1bea0596e99db1032ccd750e7028f685");

    block_template.coinbase_aux_flags = "";
    block_template.coinbase_value = 2378234;

    block_template.target =
        Parse256(
            "7fffff00000000000000000000000000"
            "00000000000000000000000000000000");

    block_template.bits = 0x207fffffU;
    block_template.minimum_time = 1788566401U;
    block_template.current_time = 1789962799U;
    block_template.height = 1;
    block_template.nonce_min = 0;
    block_template.nonce_max =
        std::numeric_limits<std::uint32_t>::max();
    block_template.sigop_limit = 80000;
    block_template.size_limit = 1048576;
    block_template.weight_limit = 4194304;

    block_template.witness_commitment_hex =
        "6a24aa21a9ed"
        "e2f61c3f71d1defd3fa999dfa3695375"
        "5c690689799962b48bebd836974e8cf9";

    return block_template;
}

} // namespace

int main()
{
    using mercaminer::BlockTemplate;
    using mercaminer::BuildBlockCandidate;
    using mercaminer::Bytes;
    using mercaminer::ComputeMerkleRoot;
    using mercaminer::DoubleSha256;
    using mercaminer::TemplateTransaction;

    bool ok{true};

    const Bytes payout =
        PayoutScript();

    const Bytes extranonce{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08};

    const BlockTemplate block_template =
        BaseTemplate();

    const auto candidate =
        BuildBlockCandidate(
            block_template,
            payout,
            extranonce);

    ok &= Check(
        candidate.coinbase.txid.ToHexBE() ==
            "15fda1757d333bea6c267ba5fc24cd6d"
            "66ba0666e998b894e9d0e95d18125af1",
        "candidate coinbase txid");

    ok &= Check(
        candidate.header.merkle_root ==
            candidate.coinbase.txid,
        "single-transaction merkle root equals coinbase txid");

    ok &= Check(
        candidate.header.previous_block ==
            block_template.previous_block_hash,
        "header previous block comes from template");

    ok &= Check(
        candidate.header.version ==
            block_template.version &&
            candidate.header.time ==
                block_template.current_time &&
            candidate.header.bits ==
                block_template.bits &&
            candidate.header.nonce ==
                block_template.nonce_min,
        "header scalar fields come from template");

    const auto serialized_header =
        candidate.header.Serialize();

    ok &= Check(
        Hex(serialized_header) ==
            "00000020"
            "85f628700e75cd2c03b19de99605ea1b"
            "d80ecc294344696e126ba1b3ef08238e"
            "f15a12185de9d0e994b898e96606ba66"
            "6dcd24fca57b266cea3b337d75a1fd15"
            "2faab06a"
            "ffff7f20"
            "00000000",
        "candidate header serialization vector");

    ok &= Check(
        candidate.serialized_block.size() == 267,
        "single-coinbase block serialized size");

    ok &= Check(
        candidate.serialized_block[80] == 0x01,
        "block transaction count is one");

    ok &= Check(
        Hex(candidate.serialized_block) ==
            "00000020"
            "85f628700e75cd2c03b19de99605ea1b"
            "d80ecc294344696e126ba1b3ef08238e"
            "f15a12185de9d0e994b898e96606ba66"
            "6dcd24fca57b266cea3b337d75a1fd15"
            "2faab06a"
            "ffff7f20"
            "00000000"
            "01"
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
            "e2f61c3f71d1defd3fa999dfa3695375"
            "5c690689799962b48bebd836974e8cf9"
            "01"
            "20"
            "00000000000000000000000000000000"
            "00000000000000000000000000000000"
            "00000000",
        "complete block serialization bytes");

    ok &= Check(
        std::equal(
            candidate.coinbase.serialized_with_witness.begin(),
            candidate.coinbase.serialized_with_witness.end(),
            candidate.serialized_block.begin() + 81),
        "witness coinbase appended verbatim");

    ok &= Check(
        DoubleSha256(
            candidate.serialized_block).ToHexBE() ==
            "592f71d0e6475a9884cac78fd445c437"
            "550cee7a0848243ba6b52f4821dea25f",
        "complete block serialization vector");

    BlockTemplate transaction_template =
        block_template;

    TemplateTransaction witness_transaction;
    witness_transaction.data_hex = "00";

    // Deliberately keep txid distinct from wtxid so this test
    // proves which identifier feeds the ordinary Merkle tree.
    witness_transaction.txid =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000001");

    // SHA256d of the raw GBT transaction bytes 0x00.
    witness_transaction.wtxid =
        Parse256(
            "9a538906e6466ebd2617d321f71bc94e"
            "56056ce213d366773699e28158e00614");

    transaction_template.transactions = {
        witness_transaction};

    // BIP141 commitment for witness leaves:
    // [zero coinbase leaf, witness_transaction.wtxid],
    // with the 32-byte zero witness reserved value.
    transaction_template.witness_commitment_hex =
        "6a24aa21a9ed"
        "26cb22fab8c881457de26c7b2011e6a0"
        "16b231f2c6ae220321e4cf57ed06cd64";

    const auto transaction_candidate =
        BuildBlockCandidate(
            transaction_template,
            payout,
            extranonce);

    const auto expected_txid_merkle =
        ComputeMerkleRoot({
            transaction_candidate.coinbase.txid,
            witness_transaction.txid}).root;

    const auto wrong_wtxid_merkle =
        ComputeMerkleRoot({
            transaction_candidate.coinbase.txid,
            witness_transaction.wtxid}).root;

    ok &= Check(
        transaction_candidate.header.merkle_root ==
                expected_txid_merkle &&
            transaction_candidate.header.merkle_root !=
                wrong_wtxid_merkle,
        "ordinary merkle tree uses GBT txid, not wtxid");

    BlockTemplate bad_wtxid =
        transaction_template;

    bad_wtxid.transactions[0].wtxid =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000002");

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                bad_wtxid,
                payout,
                extranonce);
        }),
        "GBT transaction data/wtxid mismatch rejected");

    BlockTemplate bad_witness_commitment =
        transaction_template;

    bad_witness_commitment.witness_commitment_hex.back() =
        bad_witness_commitment.witness_commitment_hex.back() == '0'
            ? '1'
            : '0';

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                bad_witness_commitment,
                payout,
                extranonce);
        }),
        "GBT witness commitment mismatch rejected");

    BlockTemplate too_small =
        block_template;

    too_small.size_limit = 266;

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                too_small,
                payout,
                extranonce);
        }),
        "serialized-byte limit enforced");

    BlockTemplate tiny_weight_limit =
        block_template;

    tiny_weight_limit.weight_limit = 1;

    ok &= Check(
        !ThrowsBuild([&] {
            BuildBlockCandidate(
                tiny_weight_limit,
                payout,
                extranonce);
        }),
        "compatibility weight limit does not constrain block capacity");

    BlockTemplate bad_aux =
        block_template;

    bad_aux.coinbase_aux_flags = "0g";

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                bad_aux,
                payout,
                extranonce);
        }),
        "malformed coinbaseaux hex rejected");

    BlockTemplate bad_transaction =
        block_template;

    TemplateTransaction malformed;
    malformed.data_hex = "0g";
    malformed.txid =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000001");

    bad_transaction.transactions.push_back(
        malformed);

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                bad_transaction,
                payout,
                extranonce);
        }),
        "malformed template transaction hex rejected");

    BlockTemplate mutated =
        block_template;

    TemplateTransaction transaction_a;
    transaction_a.data_hex = "00";
    transaction_a.txid =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000001");

    TemplateTransaction transaction_b;
    transaction_b.data_hex = "00";
    transaction_b.txid =
        Parse256(
            "00000000000000000000000000000000"
            "00000000000000000000000000000002");

    mutated.transactions = {
        transaction_a,
        transaction_b,
        transaction_b};

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                mutated,
                payout,
                extranonce);
        }),
        "mutated merkle tree rejected");

    BlockTemplate huge_height =
        block_template;

    huge_height.height =
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max()) +
        1ULL;

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                huge_height,
                payout,
                extranonce);
        }),
        "unsupported height rejected");

    BlockTemplate huge_coinbase =
        block_template;

    huge_coinbase.coinbase_value =
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()) +
        1ULL;

    ok &= Check(
        ThrowsBuild([&] {
            BuildBlockCandidate(
                huge_coinbase,
                payout,
                extranonce);
        }),
        "coinbase value overflow rejected");

    return ok ? 0 : 1;
}
