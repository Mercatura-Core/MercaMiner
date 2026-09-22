// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_NETWORK_H
#define MERCAMINER_NETWORK_H

#include <gbt.h>

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace mercaminer {

class NetworkException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct NetworkIdentity
{
    std::string_view chain;
    std::uint16_t default_rpc_port;
    std::string_view data_dir;
    std::string_view genesis_hash;
    std::string_view pow_limit;
};

const NetworkIdentity*
FindNetworkIdentity(std::string_view chain) noexcept;

bool SupportsDirectPowMining(
    const NetworkIdentity& network) noexcept;

void ValidateNetworkIdentity(
    const NetworkIdentity& expected,
    const BlockchainInfo& blockchain,
    const UInt256& live_genesis);

void ValidateProofOfWorkTarget(
    const NetworkIdentity& network,
    const UInt256& target);

} // namespace mercaminer

#endif // MERCAMINER_NETWORK_H
