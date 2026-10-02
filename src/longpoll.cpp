// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <longpoll.h>

#include <gbt.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <utility>

namespace mercaminer {

nlohmann::json BuildLongpollTemplateRequest(
    std::string_view longpoll_id)
{
    if (longpoll_id.empty()) {
        throw std::invalid_argument(
            "longpoll id must not be empty");
    }

    return nlohmann::json{
        {
            "rules",
            nlohmann::json::array(
                {"segwit"})
        },
        {
            "longpollid",
            std::string{longpoll_id}
        }
    };
}

bool LongpollTemplateChangesTip(
    const UInt256& expected_previous_block_hash,
    const UInt256& replacement_previous_block_hash) noexcept
{
    return
        expected_previous_block_hash !=
        replacement_previous_block_hash;
}

bool LongpollTemplateInvalidatesOldWork(
    const UInt256& expected_previous_block_hash,
    const UInt256& replacement_previous_block_hash,
    bool submit_old) noexcept
{
    return
        LongpollTemplateChangesTip(
            expected_previous_block_hash,
            replacement_previous_block_hash) ||
        !submit_old;
}

LongpollWatcher::LongpollWatcher(
    std::string rpc_url,
    RpcCredentials credentials,
    std::string longpoll_id,
    UInt256 expected_previous_block_hash)
    : m_rpc_url(std::move(rpc_url)),
      m_credentials(std::move(credentials)),
      m_longpoll_id(std::move(longpoll_id)),
      m_expected_previous_block_hash(
          std::move(expected_previous_block_hash))
{
    if (m_longpoll_id.empty()) {
        throw std::invalid_argument(
            "longpoll id must not be empty");
    }

    m_thread =
        std::thread(
            &LongpollWatcher::Run,
            this);
}

LongpollWatcher::~LongpollWatcher()
{
    CancelAndJoin();
}

void LongpollWatcher::Run()
{
    try {
        RpcClient rpc{
            m_rpc_url,
            m_credentials};

        RpcCallOptions options;
        options.timeout_seconds = 0;
        options.cancelled =
            &m_cancel_requested;

        const auto response =
            rpc.Call(
                "getblocktemplate",
                nlohmann::json::array(
                    {
                        BuildLongpollTemplateRequest(
                            m_longpoll_id)
                    }),
                options);

        // Distinguish hard invalidation of old work from a
        // same-tip template refresh. Same-tip work remains useful
        // unless the server explicitly returns submitold=false.
        const BlockTemplate replacement_template =
            ParseBlockTemplate(response);

        const bool tip_changed =
            LongpollTemplateChangesTip(
                m_expected_previous_block_hash,
                replacement_template.previous_block_hash);

        m_tip_changed.store(
            tip_changed,
            std::memory_order_relaxed);

        m_hard_cancel.store(
            LongpollTemplateInvalidatesOldWork(
                m_expected_previous_block_hash,
                replacement_template.previous_block_hash,
                replacement_template.submit_old.value_or(true)),
            std::memory_order_relaxed);

        if (m_cancel_requested.load(
                std::memory_order_relaxed)) {
            m_status =
                LongpollStatus::CANCELLED;

            return;
        }

        m_status =
            LongpollStatus::TEMPLATE_CHANGED;

        m_refresh_requested.store(
            true,
            std::memory_order_relaxed);
    } catch (const RpcCancelledException&) {
        if (m_cancel_requested.load(
                std::memory_order_relaxed)) {
            m_status =
                LongpollStatus::CANCELLED;

            return;
        }

        m_error =
            "longpoll RPC cancelled unexpectedly";

        m_status =
            LongpollStatus::RPC_ERROR;

        m_refresh_requested.store(
            true,
            std::memory_order_relaxed);
    } catch (const std::exception& error) {
        m_error = error.what();

        m_status =
            LongpollStatus::RPC_ERROR;

        m_refresh_requested.store(
            true,
            std::memory_order_relaxed);
    }
}

void LongpollWatcher::CancelAndJoin() noexcept
{
    m_cancel_requested.store(
        true,
        std::memory_order_relaxed);

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

LongpollStatus LongpollWatcher::Finish()
{
    CancelAndJoin();
    return m_status;
}

} // namespace mercaminer
