// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef MERCAMINER_LONGPOLL_H
#define MERCAMINER_LONGPOLL_H

#include <rpc.h>
#include <uint256.h>

#include <nlohmann/json_fwd.hpp>

#include <atomic>
#include <string>
#include <string_view>
#include <thread>

namespace mercaminer {

enum class LongpollStatus
{
    RUNNING,
    TEMPLATE_CHANGED,
    CANCELLED,
    RPC_ERROR,
};

nlohmann::json BuildLongpollTemplateRequest(
    std::string_view longpoll_id);

bool LongpollTemplateChangesTip(
    const UInt256& expected_previous_block_hash,
    const UInt256& replacement_previous_block_hash) noexcept;

class LongpollWatcher
{
public:
    LongpollWatcher(
        std::string rpc_url,
        RpcCredentials credentials,
        std::string longpoll_id,
        UInt256 expected_previous_block_hash);

    ~LongpollWatcher();

    LongpollWatcher(
        const LongpollWatcher&) = delete;

    LongpollWatcher& operator=(
        const LongpollWatcher&) = delete;

    const std::atomic_bool* StaleFlag() const noexcept
    {
        return &m_stale;
    }

    bool IsStale() const noexcept
    {
        return m_stale.load(
            std::memory_order_relaxed);
    }

    LongpollStatus Finish();

    bool TipChanged() const noexcept
    {
        return m_tip_changed.load(
            std::memory_order_relaxed);
    }

    const std::string& Error() const noexcept
    {
        return m_error;
    }

private:
    void Run();
    void CancelAndJoin() noexcept;

    std::string m_rpc_url;
    RpcCredentials m_credentials;
    std::string m_longpoll_id;
    UInt256 m_expected_previous_block_hash;

    std::atomic_bool m_cancel_requested{false};
    std::atomic_bool m_stale{false};
    std::atomic_bool m_tip_changed{false};

    LongpollStatus m_status{
        LongpollStatus::RUNNING};

    std::string m_error;
    std::thread m_thread;
};

} // namespace mercaminer

#endif // MERCAMINER_LONGPOLL_H
