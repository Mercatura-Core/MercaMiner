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

LongpollWatcher::LongpollWatcher(
    std::string rpc_url,
    RpcCredentials credentials,
    std::string longpoll_id)
    : m_rpc_url(std::move(rpc_url)),
      m_credentials(std::move(credentials)),
      m_longpoll_id(std::move(longpoll_id))
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

        // Validate that a normal longpoll return is still
        // a complete Mercatura block template.
        (void)ParseBlockTemplate(response);

        if (m_cancel_requested.load(
                std::memory_order_relaxed)) {
            m_status =
                LongpollStatus::CANCELLED;

            return;
        }

        m_status =
            LongpollStatus::TEMPLATE_CHANGED;

        m_stale.store(
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

        m_stale.store(
            true,
            std::memory_order_relaxed);
    } catch (const std::exception& error) {
        m_error = error.what();

        m_status =
            LongpollStatus::RPC_ERROR;

        m_stale.store(
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
