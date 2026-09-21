// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <mining_job.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

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

mercaminer::BlockTemplate TestTemplate()
{
    mercaminer::BlockTemplate block_template;

    block_template.version = 536870912;

    block_template.previous_block_hash =
        Parse256(
            "51d63ee9e73af41a5b3aae8417b5e770"
            "e47567f16e541f1594a28e52a3e09f36");

    block_template.coinbase_aux_flags = "";
    block_template.coinbase_value = 2378234;

    block_template.target =
        Parse256(
            "7fffff00000000000000000000000000"
            "00000000000000000000000000000000");

    block_template.bits = 0x207fffffU;
    block_template.minimum_time = 1789971850U;
    block_template.current_time = 1789971850U;
    block_template.height = 3;
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

mercaminer::Bytes TestPayoutScript()
{
    mercaminer::Bytes script{
        0x52,
        0x20};

    for (unsigned int i = 0;
         i < 32;
         ++i) {
        script.push_back(
            static_cast<unsigned char>(i));
    }

    return script;
}

template <typename Callable>
bool ThrowsMiningJob(
    Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::MiningJobException&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    using mercaminer::EncodeExtranonce64;
    using mercaminer::MiningJob;

    bool ok{true};

    ok &= Check(
        EncodeExtranonce64(0) ==
            mercaminer::Bytes(
                8,
                0x00),
        "zero extranonce encodes as eight zero bytes");

    ok &= Check(
        EncodeExtranonce64(
            0x0102030405060708ULL) ==
            mercaminer::Bytes({
                0x08,
                0x07,
                0x06,
                0x05,
                0x04,
                0x03,
                0x02,
                0x01}),
        "extranonce uses fixed eight-byte little endian");

    const auto block_template =
        TestTemplate();

    const auto payout_script =
        TestPayoutScript();

    MiningJob job{
        block_template,
        payout_script,
        0};

    auto first =
        job.NextCandidate();

    auto second =
        job.NextCandidate();

    ok &= Check(
        first.extranonce == 0 &&
            second.extranonce == 1,
        "job advances extranonce monotonically");

    ok &= Check(
        first.candidate.coinbase.txid !=
            second.candidate.coinbase.txid,
        "new extranonce changes coinbase txid");

    ok &= Check(
        first.candidate.header.merkle_root !=
            second.candidate.header.merkle_root,
        "new extranonce changes merkle root");

    ok &= Check(
        first.candidate.header.previous_block ==
                second.candidate.header.previous_block &&
            first.candidate.header.time ==
                second.candidate.header.time &&
            first.candidate.header.bits ==
                second.candidate.header.bits,
        "candidate rebuild preserves template header fields");

    ok &= Check(
        first.candidate.serialized_block !=
            second.candidate.serialized_block,
        "candidate rebuild changes serialized block");

    MiningJob deterministic_a{
        block_template,
        payout_script,
        17};

    MiningJob deterministic_b{
        block_template,
        payout_script,
        17};

    const auto deterministic_first =
        deterministic_a.NextCandidate();

    const auto deterministic_second =
        deterministic_b.NextCandidate();

    ok &= Check(
        deterministic_first.extranonce ==
                deterministic_second.extranonce &&
            deterministic_first.candidate.serialized_block ==
                deterministic_second.candidate.serialized_block,
        "same template and extranonce rebuild deterministically");

    MiningJob final_job{
        block_template,
        payout_script,
        std::numeric_limits<std::uint64_t>::max()};

    const auto final_candidate =
        final_job.NextCandidate();

    ok &= Check(
        final_candidate.extranonce ==
                std::numeric_limits<std::uint64_t>::max() &&
            !final_job.HasMoreCandidates(),
        "maximum extranonce is usable exactly once");

    ok &= Check(
        ThrowsMiningJob([&] {
            final_job.NextCandidate();
        }),
        "extranonce exhaustion fails closed");

    ok &= Check(
        ThrowsMiningJob([&] {
            MiningJob invalid{
                block_template,
                {},
                0};
            (void)invalid;
        }),
        "empty payout script rejected");

    return ok ? 0 : 1;
}
