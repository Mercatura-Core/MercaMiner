// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <gbt.h>

#include <target.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mercaminer {
namespace {

[[noreturn]] void Fail(
    std::string_view field,
    std::string_view reason)
{
    throw TemplateException(
        "Invalid " + std::string{field} +
        ": " + std::string{reason});
}

const nlohmann::json& RequireMember(
    const nlohmann::json& object,
    std::string_view name)
{
    const auto it = object.find(std::string{name});

    if (it == object.end()) {
        Fail(name, "missing field");
    }

    return *it;
}

std::string RequireString(
    const nlohmann::json& object,
    std::string_view name)
{
    const auto& value = RequireMember(object, name);

    if (!value.is_string()) {
        Fail(name, "expected string");
    }

    return value.get<std::string>();
}

std::uint64_t UnsignedInteger(
    const nlohmann::json& value,
    std::string_view field)
{
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>();
    }

    if (value.is_number_integer()) {
        const std::int64_t signed_value =
            value.get<std::int64_t>();

        if (signed_value < 0) {
            Fail(field, "must not be negative");
        }

        return static_cast<std::uint64_t>(
            signed_value);
    }

    Fail(field, "expected integer");
}

std::uint64_t RequireUnsigned(
    const nlohmann::json& object,
    std::string_view name)
{
    return UnsignedInteger(
        RequireMember(object, name),
        name);
}

std::int64_t SignedInteger(
    const nlohmann::json& value,
    std::string_view field)
{
    if (value.is_number_unsigned()) {
        const std::uint64_t unsigned_value =
            value.get<std::uint64_t>();

        if (unsigned_value >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max())) {
            Fail(field, "integer out of range");
        }

        return static_cast<std::int64_t>(
            unsigned_value);
    }

    if (value.is_number_integer()) {
        return value.get<std::int64_t>();
    }

    Fail(field, "expected integer");
}

std::int64_t RequireSigned(
    const nlohmann::json& object,
    std::string_view name)
{
    return SignedInteger(
        RequireMember(object, name),
        name);
}

std::uint32_t RequireUint32(
    const nlohmann::json& object,
    std::string_view name)
{
    const std::uint64_t value =
        RequireUnsigned(object, name);

    if (value >
        std::numeric_limits<std::uint32_t>::max()) {
        Fail(name, "integer out of uint32 range");
    }

    return static_cast<std::uint32_t>(value);
}

std::int32_t RequireInt32(
    const nlohmann::json& object,
    std::string_view name)
{
    const std::int64_t value =
        RequireSigned(object, name);

    if (value <
            std::numeric_limits<std::int32_t>::min() ||
        value >
            std::numeric_limits<std::int32_t>::max()) {
        Fail(name, "integer out of int32 range");
    }

    return static_cast<std::int32_t>(value);
}

bool IsHexDigit(char c)
{
    return
        (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F');
}

bool IsEvenHex(std::string_view value)
{
    return
        !value.empty() &&
        value.size() % 2 == 0 &&
        std::all_of(
            value.begin(),
            value.end(),
            IsHexDigit);
}

UInt256 RequireUInt256(
    const nlohmann::json& object,
    std::string_view name)
{
    const std::string text =
        RequireString(object, name);

    const auto value =
        UInt256::FromHexBE(text);

    if (!value) {
        Fail(
            name,
            "expected exactly 64 hexadecimal characters");
    }

    return *value;
}

std::vector<std::string> ParseRules(
    const nlohmann::json& object)
{
    const auto& rules_json =
        RequireMember(object, "rules");

    if (!rules_json.is_array()) {
        Fail("rules", "expected array");
    }

    std::vector<std::string> rules;

    for (const auto& value : rules_json) {
        if (!value.is_string()) {
            Fail(
                "rules",
                "every rule must be a string");
        }

        rules.push_back(
            value.get<std::string>());
    }

    return rules;
}

bool RuleMatches(
    std::string_view candidate,
    std::string_view wanted)
{
    if (!candidate.empty() &&
        candidate.front() == '!') {
        candidate.remove_prefix(1);
    }

    return candidate == wanted;
}

std::string NormalizeLower(
    std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return value;
}

void ValidateWitnessCommitment(
    const std::string& commitment)
{
    if (commitment.size() != 76 ||
        !IsEvenHex(commitment)) {
        Fail(
            "default_witness_commitment",
            "expected 38-byte hexadecimal script");
    }

    const std::string lower =
        NormalizeLower(commitment);

    if (lower.rfind("6a24aa21a9ed", 0) != 0) {
        Fail(
            "default_witness_commitment",
            "unexpected witness commitment prefix");
    }
}

std::vector<TemplateTransaction>
ParseTransactions(const nlohmann::json& object)
{
    const auto& transactions_json =
        RequireMember(object, "transactions");

    if (!transactions_json.is_array()) {
        Fail("transactions", "expected array");
    }

    std::vector<TemplateTransaction> transactions;
    transactions.reserve(transactions_json.size());

    for (std::size_t i = 0;
         i < transactions_json.size();
         ++i) {
        const auto& tx = transactions_json[i];

        if (!tx.is_object()) {
            Fail(
                "transactions",
                "transaction entry must be object");
        }

        TemplateTransaction parsed;

        parsed.data_hex =
            RequireString(tx, "data");

        if (!IsEvenHex(parsed.data_hex)) {
            Fail(
                "transactions.data",
                "expected non-empty even-length hexadecimal");
        }

        parsed.txid =
            RequireUInt256(tx, "txid");

        parsed.wtxid =
            RequireUInt256(tx, "hash");

        const auto& depends =
            RequireMember(tx, "depends");

        if (!depends.is_array()) {
            Fail(
                "transactions.depends",
                "expected array");
        }

        for (const auto& dependency : depends) {
            const std::uint64_t index =
                UnsignedInteger(
                    dependency,
                    "transactions.depends");

            if (index == 0 || index > i) {
                Fail(
                    "transactions.depends",
                    "dependency must refer to an earlier transaction");
            }

            parsed.depends.push_back(
                static_cast<std::size_t>(index));
        }

        parsed.fee =
            RequireSigned(tx, "fee");

        parsed.sigops =
            RequireUnsigned(tx, "sigops");

        parsed.weight =
            RequireUnsigned(tx, "weight");

        transactions.push_back(
            std::move(parsed));
    }

    return transactions;
}

} // namespace

BlockchainInfo ParseBlockchainInfo(
    const nlohmann::json& value)
{
    if (!value.is_object()) {
        throw TemplateException(
            "Invalid getblockchaininfo: expected object");
    }

    BlockchainInfo info;

    info.chain =
        RequireString(value, "chain");

    if (info.chain.empty()) {
        Fail("chain", "must not be empty");
    }

    info.blocks =
        RequireUnsigned(value, "blocks");

    info.headers =
        RequireUnsigned(value, "headers");

    info.best_block_hash =
        RequireUInt256(
            value,
            "bestblockhash");

    const std::string bits_text =
        RequireString(value, "bits");

    const auto bits =
        ParseBitsHex(bits_text);

    if (!bits) {
        Fail(
            "bits",
            "expected exactly 8 hexadecimal characters");
    }

    info.bits = *bits;

    info.target =
        RequireUInt256(value, "target");

    const auto derived =
        ValidTargetFromCompact(info.bits);

    if (!derived) {
        Fail(
            "bits",
            "compact target is invalid");
    }

    if (*derived != info.target) {
        Fail(
            "target",
            "does not agree with bits");
    }

    return info;
}

bool HasTemplateRule(
    const BlockTemplate& block_template,
    const std::string& rule)
{
    return std::any_of(
        block_template.rules.begin(),
        block_template.rules.end(),
        [&](const std::string& candidate) {
            return RuleMatches(
                candidate,
                rule);
        });
}

BlockTemplate ParseBlockTemplate(
    const nlohmann::json& value)
{
    if (!value.is_object()) {
        throw TemplateException(
            "Invalid getblocktemplate: expected object");
    }

    BlockTemplate result;

    result.version =
        RequireInt32(value, "version");

    result.rules =
        ParseRules(value);

    if (!HasTemplateRule(result, "segwit")) {
        Fail(
            "rules",
            "segwit rule is required");
    }

    result.previous_block_hash =
        RequireUInt256(
            value,
            "previousblockhash");

    result.transactions =
        ParseTransactions(value);

    const auto& coinbase_aux =
        RequireMember(value, "coinbaseaux");

    if (!coinbase_aux.is_object()) {
        Fail(
            "coinbaseaux",
            "expected object");
    }

    const auto flags_it =
        coinbase_aux.find("flags");

    if (flags_it != coinbase_aux.end()) {
        if (!flags_it->is_string()) {
            Fail(
                "coinbaseaux.flags",
                "expected hexadecimal string");
        }

        result.coinbase_aux_flags =
            flags_it->get<std::string>();

        if (!result.coinbase_aux_flags.empty() &&
            !IsEvenHex(result.coinbase_aux_flags)) {
            Fail(
                "coinbaseaux.flags",
                "expected even-length hexadecimal");
        }
    }

    result.coinbase_value =
        RequireUnsigned(
            value,
            "coinbasevalue");

    result.longpoll_id =
        RequireString(
            value,
            "longpollid");

    if (result.longpoll_id.empty()) {
        Fail(
            "longpollid",
            "must not be empty");
    }

    const std::string bits_text =
        RequireString(value, "bits");

    const auto bits =
        ParseBitsHex(bits_text);

    if (!bits) {
        Fail(
            "bits",
            "expected exactly 8 hexadecimal characters");
    }

    result.bits = *bits;

    const auto derived_target =
        ValidTargetFromCompact(result.bits);

    if (!derived_target) {
        Fail(
            "bits",
            "compact target is invalid");
    }

    result.target =
        RequireUInt256(value, "target");

    if (*derived_target != result.target) {
        Fail(
            "target",
            "does not agree with bits");
    }

    result.minimum_time =
        RequireUint32(value, "mintime");

    result.current_time =
        RequireUint32(value, "curtime");

    if (result.current_time <
        result.minimum_time) {
        Fail(
            "curtime",
            "must be greater than or equal to mintime");
    }

    result.height =
        RequireUnsigned(value, "height");

    if (result.height == 0) {
        Fail(
            "height",
            "must be greater than zero");
    }

    const std::string nonce_range =
        RequireString(value, "noncerange");

    if (nonce_range.size() != 16 ||
        !IsEvenHex(nonce_range)) {
        Fail(
            "noncerange",
            "expected exactly 16 hexadecimal characters");
    }

    const auto nonce_min =
        ParseBitsHex(
            nonce_range.substr(0, 8));

    const auto nonce_max =
        ParseBitsHex(
            nonce_range.substr(8, 8));

    if (!nonce_min || !nonce_max) {
        Fail(
            "noncerange",
            "invalid hexadecimal range");
    }

    result.nonce_min = *nonce_min;
    result.nonce_max = *nonce_max;

    if (result.nonce_min >
        result.nonce_max) {
        Fail(
            "noncerange",
            "minimum exceeds maximum");
    }

    result.sigop_limit =
        RequireUnsigned(
            value,
            "sigoplimit");

    result.size_limit =
        RequireUnsigned(
            value,
            "sizelimit");

    if (result.size_limit == 0) {
        Fail(
            "sizelimit",
            "must be greater than zero");
    }

    const auto weight_it =
        value.find("weightlimit");

    if (weight_it != value.end()) {
        const std::uint64_t weight_limit =
            UnsignedInteger(
                *weight_it,
                "weightlimit");

        if (weight_limit == 0) {
            Fail(
                "weightlimit",
                "must be greater than zero");
        }

        result.weight_limit =
            weight_limit;
    }

    result.witness_commitment_hex =
        RequireString(
            value,
            "default_witness_commitment");

    ValidateWitnessCommitment(
        result.witness_commitment_hex);

    return result;
}

} // namespace mercaminer
