/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_HEARTBEAT_CPP
#define JIYAN_SDK_HEARTBEAT_CPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace JiYanSDK_Heartbeat
{
    static std::thread g_thread;
    static std::atomic<bool> g_stop{true};
    static std::atomic<bool> g_running{false};
    static std::mutex g_mtx;
    static std::condition_variable g_cv;

    static std::string g_card;
    static std::string g_device;

    const int HB_MAX_FAIL = 2;

    void hb_loop(int interval_seconds)
    {
        int interval = interval_seconds;
        if (interval < 1) interval = 30;
        if (interval > 1440) interval = 1440;

        int fail = 0;

        while (!g_stop.load())
        {
            {
                std::unique_lock<std::mutex> lk(g_mtx);
                g_cv.wait_for(lk, std::chrono::minutes(interval), [] { return g_stop.load(); });
            }
            if (g_stop.load()) break;

            const std::string card = g_card;
            const std::string dev = g_device;
            if (card.empty()) break;

            bool ok = false;
            std::string msg;

            std::lock_guard<std::recursive_mutex> request_lock(JiYanSDK::RequestMutex());
            JiYanSDK::set_php("key.php");
            nlohmann::json params = {{"card_key", card}};
            const auto runtime = JiYanSDK::GetRuntimeSnapshot();
            if (!runtime.verified || runtime.card_key != card || runtime.device_id != dev) break;
            if (!runtime.player_uid.empty()) params["user_uid"] = runtime.player_uid;
            if (!runtime.player_name.empty()) params["user_name"] = runtime.player_name;
            if (!dev.empty())
            {
                params["device_id"] = dev;
            }
            params["timestamp"] = JiYanSDK_Function::Get_Timestamp();
            params["nonce"] = JiYanSDK_Function::Random_String(16);
            if (!JiYanSDK::account_info.Account_Pass.empty()) params["password"] = JiYanSDK::account_info.Account_Pass;
            if (!JiYanSDK::account_info.Account_Token.empty()) params["token"] = JiYanSDK::account_info.Account_Token;

            const std::string raw = API_Request("heartbeat", params);
            try
            {
                nlohmann::json res = nlohmann::json::parse(raw);
                ok = JiYan_Last_Request_Ok;
                msg = res.value("message", std::string());
            }
            catch (const std::exception& e)
            {
                ok = JiYan_Last_Request_Ok;
                msg = raw;
            }

            if (ok)
            {
                const auto current = JiYanSDK::GetRuntimeSnapshot();
                if (current.generation == runtime.generation && current.player_uid == runtime.player_uid &&
                    current.player_name == runtime.player_name)
                    JiYanSDK::MarkIdentityUploaded(runtime.generation);
                fail = 0;
                const int next = JiYanSDK_Result::Data.session.heartbeat_interval;
                if (next >= 1 && next <= 1440) interval = next;
                continue;
            }

            ++fail;

            // 历史响应签名不覆盖顶层code；会话失效仅由本地连续失败策略决定。
            if (fail >= HB_MAX_FAIL)
            {
                JiYanSDK::set_locked(true);
                break;
            }
        }

        g_running.store(false);
    }
}

void Start_Heartbeat(const std::string& card_key, const std::string& device_id, int interval_seconds)
{
    Stop_Heartbeat();

    JiYanSDK_Heartbeat::g_card = card_key;
    JiYanSDK_Heartbeat::g_device = device_id.empty() ? JiYanSDK::Device_Id : device_id;
    JiYanSDK_Heartbeat::g_stop.store(false);
    JiYanSDK_Heartbeat::g_running.store(true);

    JiYanSDK_Heartbeat::g_thread = std::thread(JiYanSDK_Heartbeat::hb_loop, interval_seconds);
}

void Stop_Heartbeat()
{
    JiYanSDK_Heartbeat::g_stop.store(true);
    {
        std::lock_guard<std::mutex> lk(JiYanSDK_Heartbeat::g_mtx);
    }
    JiYanSDK_Heartbeat::g_cv.notify_all();

    if (JiYanSDK_Heartbeat::g_thread.joinable())
    {
        if (JiYanSDK_Heartbeat::g_thread.get_id() != std::this_thread::get_id())
        {
            JiYanSDK_Heartbeat::g_thread.join();
        }
        else
        {
            JiYanSDK_Heartbeat::g_thread.detach();
        }
    }
    JiYanSDK_Heartbeat::g_running.store(false);
}

#endif