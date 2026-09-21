// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <gbt.h>

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
bool ThrowsTemplate(Callable&& callable)
{
    try {
        callable();
    } catch (const mercaminer::TemplateException&) {
        return true;
    }

    return false;
}

nlohmann::json RegtestBlockchainInfo()
{
    return nlohmann::json::parse(R"(
{
  "chain": "regtest",
  "blocks": 0,
  "headers": 0,
  "bestblockhash": "8e2308efb3a16b126e69444329cc0ed81bea0596e99db1032ccd750e7028f685",
  "bits": "207fffff",
  "target": "7fffff0000000000000000000000000000000000000000000000000000000000"
}
)");
}

nlohmann::json RegtestTemplate()
{
    return nlohmann::json::parse(R"(
{
  "capabilities": ["proposal"],
  "version": 536870912,
  "rules": ["csv", "!segwit", "taproot"],
  "vbavailable": {},
  "vbrequired": 0,
  "previousblockhash": "8e2308efb3a16b126e69444329cc0ed81bea0596e99db1032ccd750e7028f685",
  "transactions": [],
  "coinbaseaux": {},
  "coinbasevalue": 2378234,
  "longpollid": "8e2308efb3a16b126e69444329cc0ed81bea0596e99db1032ccd750e7028f6851",
  "target": "7fffff0000000000000000000000000000000000000000000000000000000000",
  "mintime": 1788566401,
  "mutable": ["time", "transactions", "prevblock"],
  "noncerange": "00000000ffffffff",
  "sigoplimit": 80000,
  "sizelimit": 1048576,
  "weightlimit": 4194304,
  "curtime": 1789962799,
  "bits": "207fffff",
  "height": 1,
  "default_witness_commitment": "6a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf9"
}
)");
}

} // namespace

int main()
{
    using mercaminer::HasTemplateRule;
    using mercaminer::ParseBlockchainInfo;
    using mercaminer::ParseBlockTemplate;

    bool ok{true};

    const auto chain =
        ParseBlockchainInfo(
            RegtestBlockchainInfo());

    ok &= Check(
        chain.chain == "regtest",
        "regtest chain parsed");

    ok &= Check(
        chain.blocks == 0 &&
            chain.headers == 0,
        "regtest chain heights parsed");

    ok &= Check(
        chain.best_block_hash.ToHexBE() ==
            "8e2308efb3a16b126e69444329cc0ed8"
            "1bea0596e99db1032ccd750e7028f685",
        "best block hash parsed");

    ok &= Check(
        chain.bits == 0x207fffffU &&
            chain.target.ToHexBE() ==
                "7fffff00000000000000000000000000"
                "00000000000000000000000000000000",
        "chain bits and target agree");

    auto bad_chain_target =
        RegtestBlockchainInfo();

    bad_chain_target["target"] =
        std::string(64, '0');

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockchainInfo(
                bad_chain_target);
        }),
        "chain target mismatch rejected");

    const auto block_template =
        ParseBlockTemplate(
            RegtestTemplate());

    ok &= Check(
        block_template.version == 536870912,
        "template version parsed");

    ok &= Check(
        HasTemplateRule(
            block_template,
            "segwit"),
        "mandatory !segwit rule recognized");

    ok &= Check(
        block_template.previous_block_hash ==
            chain.best_block_hash,
        "template previous block matches chain tip");

    ok &= Check(
        block_template.transactions.empty(),
        "empty transaction template parsed");

    ok &= Check(
        block_template.coinbase_value == 2378234,
        "coinbase value parsed exactly");

    ok &= Check(
        block_template.bits == 0x207fffffU &&
            block_template.target ==
                chain.target,
        "template bits and full target agree");

    ok &= Check(
        block_template.nonce_min == 0 &&
            block_template.nonce_max ==
                0xffffffffU,
        "full nonce range parsed");

    ok &= Check(
        block_template.size_limit == 1048576,
        "serialized-byte size limit parsed");

    ok &= Check(
        block_template.weight_limit.has_value() &&
            *block_template.weight_limit ==
                4194304,
        "compatibility weight limit parsed");

    ok &= Check(
        block_template.height == 1,
        "template height parsed");

    auto no_segwit =
        RegtestTemplate();
    no_segwit["rules"] =
        nlohmann::json::array(
            {"csv", "taproot"});

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                no_segwit);
        }),
        "template without segwit rejected");

    auto bad_target =
        RegtestTemplate();
    bad_target["target"] =
        std::string(64, '0');

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                bad_target);
        }),
        "template target mismatch rejected");

    auto bad_bits =
        RegtestTemplate();
    bad_bits["bits"] =
        "zzzzzzzz";

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                bad_bits);
        }),
        "malformed bits rejected");

    auto bad_prevhash =
        RegtestTemplate();
    bad_prevhash["previousblockhash"] =
        "abcd";

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                bad_prevhash);
        }),
        "malformed previous block hash rejected");

    auto bad_nonce_range =
        RegtestTemplate();
    bad_nonce_range["noncerange"] =
        "ffffffff00000000";

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                bad_nonce_range);
        }),
        "reversed nonce range rejected");

    auto bad_time =
        RegtestTemplate();
    bad_time["curtime"] = 100;
    bad_time["mintime"] = 101;

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                bad_time);
        }),
        "curtime below mintime rejected");

    auto bad_commitment =
        RegtestTemplate();
    bad_commitment[
        "default_witness_commitment"] =
            "6a00";

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                bad_commitment);
        }),
        "malformed witness commitment rejected");

    auto transaction_template =
        RegtestTemplate();

    transaction_template["transactions"] =
        nlohmann::json::array({
            {
                {"data", "00"},
                {"txid",
                 std::string(64, '0')},
                {"hash",
                 std::string(64, '1')},
                {"depends",
                 nlohmann::json::array()},
                {"fee", 1},
                {"sigops", 4},
                {"weight", 100}
            }
        });

    const auto with_transaction =
        ParseBlockTemplate(
            transaction_template);

    ok &= Check(
        with_transaction.transactions.size() == 1 &&
            with_transaction.transactions[0].data_hex ==
                "00" &&
            with_transaction.transactions[0].fee == 1 &&
            with_transaction.transactions[0].sigops == 4 &&
            with_transaction.transactions[0].weight == 100,
        "transaction metadata parsed");

    transaction_template["transactions"][0]
        ["depends"] =
            nlohmann::json::array({1});

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                transaction_template);
        }),
        "self dependency rejected");

    auto missing_commitment =
        RegtestTemplate();

    missing_commitment.erase(
        "default_witness_commitment");

    ok &= Check(
        ThrowsTemplate([&] {
            (void)ParseBlockTemplate(
                missing_commitment);
        }),
        "missing witness commitment rejected");

    return ok ? 0 : 1;
}
