// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_PAYOUT_ADDRESS_H
#define MERCAMINER_PAYOUT_ADDRESS_H

#include <rpc.h>
#include <serialization.h>

#include <nlohmann/json_fwd.hpp>

#include <stdexcept>
#include <string_view>

namespace mercaminer {

class PayoutAddressException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

Bytes ParsePayoutAddressResponse(
    const nlohmann::json& result);

Bytes ResolvePayoutAddress(
    RpcClient& rpc,
    std::string_view address);

Bytes ResolvePayoutAddress(
    RpcClient& rpc,
    std::string_view address,
    const RpcCallOptions& options);

} // namespace mercaminer

#endif // MERCAMINER_PAYOUT_ADDRESS_H
