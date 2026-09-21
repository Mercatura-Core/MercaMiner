// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <rpc_connection.h>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace mercaminer {

RpcConnectionSettings LocalRpcConnectionForHome(
    const NetworkIdentity& network,
    std::string_view home_directory)
{
    if (home_directory.empty()) {
        throw RpcConnectionException(
            "HOME directory is empty; specify RPC URL and cookie file explicitly");
    }

    std::filesystem::path cookie_path{
        std::string{home_directory}};

    cookie_path /= ".mercatura";

    if (!network.data_dir.empty()) {
        cookie_path /= std::string{network.data_dir};
    }

    cookie_path /= ".cookie";

    return RpcConnectionSettings{
        "http://127.0.0.1:" +
            std::to_string(network.default_rpc_port),
        cookie_path.string()};
}

RpcConnectionSettings DefaultLocalRpcConnection(
    const NetworkIdentity& network)
{
    const char* home = std::getenv("HOME");

    if (home == nullptr || *home == '\0') {
        throw RpcConnectionException(
            "HOME is not set; specify RPC URL and cookie file explicitly");
    }

    return LocalRpcConnectionForHome(
        network,
        home);
}

} // namespace mercaminer
