// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_RPC_CONNECTION_H
#define MERCAMINER_RPC_CONNECTION_H

#include <network.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace mercaminer {

class RpcConnectionException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct RpcConnectionSettings
{
    std::string rpc_url;
    std::string cookie_file;
};

RpcConnectionSettings LocalRpcConnectionForHome(
    const NetworkIdentity& network,
    std::string_view home_directory);

RpcConnectionSettings DefaultLocalRpcConnection(
    const NetworkIdentity& network);

} // namespace mercaminer

#endif // MERCAMINER_RPC_CONNECTION_H
