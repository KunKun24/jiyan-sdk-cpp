/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_API_CPP
#define JIYAN_SDK_API_CPP

#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cctype>

void Start_Heartbeat(const std::string& card_key, const std::string& device_id, int interval_seconds);
void Stop_Heartbeat();

namespace JiYanSDK_API
{
    static std::atomic<bool> is_requesting{false};

    nlohmann::json push_back_params(nlohmann::json params)
    {
        nlohmann::json result = params;
        const auto runtime = JiYanSDK::GetRuntimeSnapshot();
        // 玩家身份只有验证SDK这一处来源，云控请求不重复上报。
        if (!runtime.player_uid.empty() && !result.contains("user_uid")) result["user_uid"] = runtime.player_uid;
        if (!runtime.player_name.empty() && !result.contains("user_name")) result["user_name"] = runtime.player_name;
        if (!runtime.card_key.empty() && !result.contains("card_key")) result["card_key"] = runtime.card_key;

        if (!runtime.account.Account_Pass.empty())
        {
            result["password"] = runtime.account.Account_Pass;
        }
        if (!runtime.account.Account_Token.empty())
        {
            result["token"] = runtime.account.Account_Token;
        }
        if (!runtime.device_id.empty() && !result.contains("device_id"))
        {
            result["device_id"] = runtime.device_id;
        }
        // set_extra_params 追加的自定义字段（不覆盖 SDK 自身的身份字段）
        for (auto it = JiYanSDK::ExtraParams().begin(); it != JiYanSDK::ExtraParams().end(); ++it)
        {
            if (!result.contains(it.key())) result[it.key()] = it.value();
        }
        result["timestamp"] = JiYanSDK_Function::Get_Timestamp();
        result["nonce"] = JiYanSDK_Function::Random_String(16);

        return result;
    }

    void start_main(std::string api_action, nlohmann::json params, bool& is_success, std::string& result, nlohmann::json& data, bool notify_completion)
    {
        std::unique_lock<std::recursive_mutex> request_lock(JiYanSDK::RequestMutex());
        const auto sent_runtime = JiYanSDK::GetRuntimeSnapshot();
        const auto sent_params = push_back_params(params);
        std::string response = API_Request(api_action, sent_params);

        is_success = JiYan_Last_Request_Ok;
        data = nlohmann::json::object(); // 失败不能保留上次的业务数据
        result = JiYanSDK_Result::Data.message;
        JiYanSDK_Result::current_response_result = response;
        if (auto handler = JiYanSDK::GlobalResponseHandler()) handler();
        if (is_success)
        {
            try
            {
                const auto Response = nlohmann::json::parse(response);
                if (!Response.is_object()) throw std::runtime_error("响应格式错误");
                if (Response.contains("data")) data = Response["data"];
                if (Response.contains("message") && Response["message"].is_string())
                    result = Response["message"].get<std::string>();
            }
            catch (const std::exception& e)
            {
                is_success = false;
                JiYan_Last_Request_Ok = false;
                result = std::string("响应解析失败：") + e.what();
                data = nlohmann::json::object();
            }
        }
        if (result.empty()) result = is_success ? "请求成功" : "请求失败";
        // 不以未验证 JSON 的 success/message 覆盖协议层的验签失败原因。
        JiYanSDK_Result::Data.ok = is_success;
        JiYanSDK_Result::Data.message = result;

        JiYanSDK_Result::Data.raw = response;

        if (is_success && api_action == "use_card" &&
            !JiYanSDK::SetVerifiedCardIfCurrent(params.value("card_key", std::string()), sent_runtime.generation)) {
            is_success = false;
            result = "验证期间身份或设备已变更，请重新验证";
        }
        if (is_success && api_action == "unbind_device" &&
            JiYanSDK::GetRuntimeSnapshot().card_key == params.value("card_key", std::string())) {
            JiYanSDK::ClearVerifiedCard();
        }
        if (is_success && (api_action == "use_card" || api_action == "update_game_info" || api_action == "heartbeat")) {
            // 验证成功会改变generation，身份相同才能开放云控的新目标。
            const auto current = JiYanSDK::GetRuntimeSnapshot();
            if (current.player_uid == sent_params.value("user_uid", std::string()) &&
                current.player_name == sent_params.value("user_name", std::string()) &&
                current.device_id == sent_runtime.device_id &&
                (api_action == "use_card" || current.generation == sent_runtime.generation))
                JiYanSDK::MarkIdentityUploaded(current.generation);
        }
        if (!is_success) {
            data = nlohmann::json::object();
            JiYan_Last_Request_Ok = false;
            JiYanSDK_Result::Data.ok = false;
            JiYanSDK_Result::Data.message = result;
        }
        request_lock.unlock();
        if (notify_completion && JiYanSDK::Request_Completed_Callback)
        {
            JiYanSDK::Request_Completed_Callback();
        }
    }

    // 保留原五参数签名；组合更新流程显式选用带notify_completion的内部重载。
    void start_main(std::string api_action, nlohmann::json params, bool& is_success, std::string& result, nlohmann::json& data)
    {
        start_main(std::move(api_action), std::move(params), is_success, result, data, true);
    }

    void start_thread(std::string api_action, nlohmann::json params, bool& is_success, std::string& result, nlohmann::json& data, std::function<void()> data_hander = nullptr, std::function<void()> callback = nullptr)
    {
        is_requesting.store(true);
        result = "wait...";

        const std::string request_route = JiYanSDK::php_action;
        std::thread([api_action, params, request_route, &is_success, &result, &data, data_hander, callback]()
        {
            struct Finally
            {
                ~Finally() { is_requesting.store(false); }
            } finalizer;

            JiYanSDK::set_php(request_route);
            start_main(api_action, params, is_success, result, data);

            if (data_hander)
            {
                data_hander();
            }

            if (callback)
            {
                callback();
            }

        }).detach();
    }

    void start(std::string api_action, nlohmann::json params, bool& is_success, std::string& result, nlohmann::json& data, std::function<void()> data_hander = nullptr, std::function<void()> callback = nullptr)
    {
        start_main(api_action, params, is_success, result, data);

        if (data_hander)
        {
            data_hander();
        }

        if (callback)
        {
            callback();
        }
    }

    void start_(std::string api_action, nlohmann::json params, bool& is_success, std::string& result, nlohmann::json& data, std::function<void()> data_hander = nullptr, std::function<void()> callback = nullptr)
    {
        if(JiYanSDK::bUseSingleThreaded)
            start(api_action, params, is_success, result, data, data_hander, callback);
        else
            start_thread(api_action, params, is_success, result, data, data_hander, callback);
    }

    void Start_Heartbeat(const std::string& card_key, const std::string& device_id, int interval_seconds);

    void RejectManagement(bool& is_success, std::string& result, const std::function<void()>& callback)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
            is_success = false;
            result = "新 HTTP 客户端包不开放管理操作，请在 HTTPS 主站后台操作";
            JiYan_Last_Request_Ok = false;
            JiYanSDK_Result::Data.ok = false;
            JiYanSDK_Result::Data.message = result;
            JiYanSDK_Result::Data.cloud = Cloud_Info{};
        }
        if (JiYanSDK::Request_Completed_Callback) JiYanSDK::Request_Completed_Callback();
        if (callback) callback();
    }

    namespace Kami
    {
        void Use(std::string key, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            if (is_requesting.load())
            {
                return;
            }

            ::Stop_Heartbeat();
            JiYanSDK::ClearVerifiedCard();
            JiYanSDK::set_php("key.php");
            nlohmann::json params = {{"card_key", key}};
            static nlohmann::json data;

            JiYanSDK_API::start_("use_card", params, is_success, result, data, [key, &is_success]()
            {
                if (is_success)
                {
                    JiYanSDK::Reset_Lock();
                    int hb_sec = JiYanSDK_Result::Data.session.heartbeat_seconds;
                    if (hb_sec <= 0)
                    {
                        int hb_min = JiYanSDK_Result::Data.session.heartbeat_interval;
                        hb_sec = (hb_min > 0 ? hb_min : 30) * 60;
                    }
                    ::Start_Heartbeat(key, JiYanSDK::Device_Id, hb_sec);
                }
            }, callback);
        }

        void Unbind(std::string key, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            if (is_requesting.load())
            {
                return;
            }

            JiYanSDK::set_php("key.php");
            nlohmann::json params = {{"card_key", key}};
            static nlohmann::json data;

            JiYanSDK_API::start_("unbind_device", params, is_success, result, data, [&is_success]() { if (is_success) ::Stop_Heartbeat(); }, callback);
        }

        void Heartbeat(std::string key, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            JiYanSDK::set_php("key.php");
            nlohmann::json params = {{"card_key", key}};
            static nlohmann::json data;

            JiYanSDK_API::start("heartbeat", params, is_success, result, data, nullptr, callback);
        }

        void GetInfo(std::string key, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            if (is_requesting.load())
            {
                return;
            }

            JiYanSDK::set_php("key.php");
            nlohmann::json params = {{"card_key", key}};
            static nlohmann::json data;

            JiYanSDK_API::start_("get_card_info", params, is_success, result, data, nullptr, callback);
        }

        namespace Create
        {
            static std::string creator = "";
            static std::string notes = "";

            void set_creator(std::string new_creator)
            {
                creator = std::move(new_creator);
            }

            void set_notes(std::string new_notes)
            {
                notes = std::move(new_notes);
            }

            // 统一走 create_card_client：服务端验签通过、应用可用即建卡，不要求后台会话。
            static void Submit(std::string cards, std::string type, int days, int hours, int minutes, int max_devices,
                               bool& is_success, std::string& result, std::function<void()> callback)
            {
                nlohmann::json params = {
                    {"card_keys", std::move(cards)},
                    {"card_type", std::move(type)},
                    {"duration_days", days},
                    {"duration_hours", hours},
                    {"duration_minutes", minutes},
                    {"max_devices", max_devices},
                };
                if (!creator.empty()) params["creator"] = creator;
                if (!notes.empty()) params["notes"] = notes;
                nlohmann::json data = nlohmann::json::object();
                start_("create_card_client", params, is_success, result, data, nullptr, std::move(callback));
            }

            // 普通卡：天数 <= 0 表示永久
            void Ordinary(std::string cards, int days, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
            {
                Submit(std::move(cards), "1", days, 0, 0, 1, is_success, result, std::move(callback));
            }

            // 多设备卡：允许 max_devices 台设备同时使用
            void Multi_Device(std::string cards, int days, int max_devices, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
            {
                Submit(std::move(cards), "2", days, 0, 0, max_devices, is_success, result, std::move(callback));
            }

            // 单次卡：当前服务端未提供该卡种，直接给出明确错误，避免误判成功。
            void Single(std::string, int, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
            {
                is_success = false;
                result = "当前服务端未开放单次卡类型，请使用普通卡或多设备卡";
                if (callback) callback();
            }

            // 按时长：天 + 小时 + 分钟
            void ByDuration(std::string cards, int days, int hours, int minutes, int max_devices, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
            {
                Submit(std::move(cards), max_devices > 1 ? "2" : "1", days, hours, minutes, max_devices, is_success, result, std::move(callback));
            }
        }
    }

    namespace Gameinfo
    {
        static auto Last_Exe_Time = std::chrono::steady_clock::now() - std::chrono::minutes(10);
        static bool is_Refresh_Now = false;
        static std::string Last_Sent_Name, Last_Sent_Uid;

        void Refresh(std::string local_Name, std::string local_Uid, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            // 渲染线程调用：只读缓存、只在“玩家真的变了”时写一次，全程不碰网络锁，也不做网络。
            if (is_requesting.load()) return;
            if (JiYanSDK_Function::JiYan_strempty(local_Name) || JiYanSDK_Function::JiYan_strempty(local_Uid)) return;

            const auto snapshot = JiYanSDK::GetRuntimeSnapshot();
            if (!snapshot.verified) return;

            const bool same_identity = snapshot.player_name == local_Name && snapshot.player_uid == local_Uid;
            if (!same_identity) JiYanSDK::SetPlayerInfo(local_Name, local_Uid);

            auto Now = std::chrono::steady_clock::now();
            auto Time_Difference = std::chrono::duration_cast<std::chrono::minutes>(Now - Last_Exe_Time).count();

            if (!is_Refresh_Now && Time_Difference < 9 && same_identity && snapshot.identity_ready &&
                local_Name == Last_Sent_Name && local_Uid == Last_Sent_Uid)
            {
                return;
            }

            JiYanSDK::set_php("user.php");
            nlohmann::json params = {{"user_name", local_Name}, {"user_uid", local_Uid}};
            static nlohmann::json data;

            JiYanSDK_API::start_("update_game_info", params, is_success, result, data, nullptr, callback);

            Last_Exe_Time = Now;
            Last_Sent_Name = local_Name; Last_Sent_Uid = local_Uid;
            is_Refresh_Now = false;
        }

        void Refresh_Now()
        {
            is_Refresh_Now = true;
        }

        bool IsUser(const std::string& Player_Uid)
        {
            const std::vector<std::string>& list = JiYanSDK_Result::Data.online.game_uids;
            return std::find(list.begin(), list.end(), Player_Uid) != list.end();
        }
    }

    namespace CloudUpdate
    {
        namespace Detail
        {
            std::string LowerMD5(std::string value)
            {
                if (value.size() != 32) return "";
                for (char& c : value) {
                    if (!std::isxdigit(static_cast<unsigned char>(c))) return "";
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                return value;
            }

            std::string FileId(const nlohmann::json& data)
            {
                if (!data.contains("file_id")) return "";
                const auto& id = data["file_id"];
                if (id.is_string()) return id.get<std::string>();
                if (id.is_number_unsigned()) return std::to_string(id.get<unsigned long long>());
                if (id.is_number_integer()) return std::to_string(id.get<long long>());
                return "";
            }

            bool ValidId(const std::string& id)
            {
                if (id.empty() || id.size() > 19 || id[0] == '0') return false;
                return id.find_first_not_of("0123456789") == std::string::npos;
            }

            void Publish(const Cloud_Update_Result& result)
            {
                JiYanSDK_Result::Data.cloud = result.cloud;
                JiYanSDK_Result::Data.ok = result.request_ok;
                JiYanSDK_Result::Data.message = result.message;
                JiYan_Last_Request_Ok = result.request_ok;
            }

            Cloud_Update_Result Failure(const std::string& message)
            {
                Cloud_Update_Result result;
                result.message = message;
                Publish(result);
                return result;
            }

            Cloud_Update_Result Request(const std::string& action, const nlohmann::json& params)
            {
                Cloud_Update_Result result;
                nlohmann::json data;
                JiYanSDK::set_php("Cloudupdate.php");
                start_main(action, params, result.request_ok, result.message, data, false);
                if (!result.request_ok) return Failure(result.message);
                try {
                    auto& cloud = result.cloud;
                    cloud.request_ok = true;
                    cloud.need_update = data.value("need_update", 0);
                    cloud.has_file = data.value("has_file", 0);
                    cloud.file_id = FileId(data);
                    cloud.file_name = data.value("file_name", std::string());
                    cloud.file_size = data.value("file_size", 0LL);
                    cloud.latest_md5 = data.value("latest_md5", std::string());
                    cloud.version = data.value("version", std::string());
                    cloud.download_url = data.value("download_url", std::string());
                    cloud.update_content = data.value("update_content", std::string());
                    cloud.ticket = data.value("ticket", std::string());
                    cloud.source = data.value("source", std::string());
                    cloud.file_md5 = data.value("file_md5", std::string());
                    cloud.confirmed = data.value("confirmed", 0);
                    cloud.charged = data.value("charged", 0);
                    cloud.remaining_times = data.value("remaining_times", -1LL);
                } catch (const std::exception& e) {
                    return Failure(std::string("云更新响应字段错误：") + e.what());
                }
                if (action == "check_version") {
                    if (!data.contains("has_file") || !data.contains("need_update") ||
                        (result.cloud.has_file != 0 && result.cloud.has_file != 1) ||
                        (result.cloud.need_update != 0 && result.cloud.need_update != 1) ||
                        (result.cloud.has_file == 1 && (!ValidId(result.cloud.file_id) || result.cloud.file_name.empty())))
                        return Failure("云更新检查响应缺少有效的文件或更新状态");
                }
                Publish(result);
                return result;
            }

            bool SameRuntime(const JiYanSDK::RuntimeSnapshot& a, const JiYanSDK::RuntimeSnapshot& b)
            {
                return a.generation == b.generation && a.device_id == b.device_id &&
                    a.app.App_Pid == b.app.App_Pid && a.account.Account_Uid == b.account.Account_Uid &&
                    a.api_url == b.api_url && a.card_key == b.card_key &&
                    a.player_uid == b.player_uid && a.player_name == b.player_name;
            }

            bool Matches(const Cloud_Download_Snapshot& download, const JiYanSDK::RuntimeSnapshot& runtime)
            {
                return download.generation == runtime.generation && download.device_id == runtime.device_id &&
                    download.app_id == runtime.app.App_Pid && download.developer_uid == runtime.account.Account_Uid &&
                    download.api_url == runtime.api_url && download.card_key == runtime.card_key &&
                    download.player_uid == runtime.player_uid && download.player_name == runtime.player_name;
            }

            Cloud_Download_Result DownloadFailure(const std::string& message)
            {
                const auto failure = Failure(message);
                return {false, failure.message, failure.cloud, {}};
            }
        }

        // 同步接口返回独立结果，请在业务工作线程调用，避免阻塞绘制线程。
        Cloud_Update_Result CheckVersionByMD5(const std::string& file_name, const std::string& md5)
        {
            std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
            const auto normalized = Detail::LowerMD5(md5);
            if (file_name.empty() || normalized.empty()) return Detail::Failure("文件名或本地 MD5 无效");
            auto result = Detail::Request("check_version", {{"Name", file_name}, {"MD5", normalized}});
            if (result.request_ok && result.cloud.has_file && result.cloud.file_name != file_name)
                return Detail::Failure("指定文件未发布，服务端返回其他文件，请检查文件名");
            return result;
        }

        Cloud_Update_Result CheckVersionSync(const std::string& so_name)
        {
            std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
            const auto md5 = JiYanSDK_Function::Get_So_MD5(so_name);
            if (md5.empty()) return Detail::Failure("本地 SO 文件无法读取或未加载，不能进行 MD5 检查");
            return CheckVersionByMD5(so_name, md5);
        }

        Cloud_Update_Result CheckVersionByVersionSync(const std::string& version)
        {
            std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
            if (version.empty()) return Detail::Failure("本地版本号不能为空");
            return Detail::Request("check_version", {{"version", version}});
        }

        Cloud_Download_Result GetDownloadUrlById(const std::string& file_id, const std::string& expected_name = "", const std::string& expected_md5 = "")
        {
            std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
            if (!Detail::ValidId(file_id)) return Detail::DownloadFailure("file_id 必须为正整数");
            if (!expected_md5.empty() && Detail::LowerMD5(expected_md5).empty())
                return Detail::DownloadFailure("预期 MD5 无效，请重新检查版本");
            const auto runtime = JiYanSDK::GetRuntimeSnapshot();
            if (runtime.device_id.empty()) return Detail::DownloadFailure("设备码不能为空");
            auto result = Detail::Request("get_download_url", {{"file_id", file_id}, {"device_id", runtime.device_id}});
            if (!result.request_ok) return {false, result.message, result.cloud, {}};
            auto& cloud = result.cloud;
            if (!Detail::SameRuntime(runtime, JiYanSDK::GetRuntimeSnapshot()))
                return Detail::DownloadFailure("取链接期间身份、设备或应用已变更，请重新检查版本");
            const auto md5 = Detail::LowerMD5(cloud.file_md5);
            if (cloud.file_name.empty() || cloud.download_url.empty() ||
                (cloud.source != "local" && cloud.source != "remote") ||
                (cloud.source == "local" && (md5.empty() || cloud.ticket.empty())) ||
                (!expected_name.empty() && cloud.file_name != expected_name) ||
                (!expected_md5.empty() && md5 != Detail::LowerMD5(expected_md5)) ||
                (!cloud.file_id.empty() && cloud.file_id != file_id))
                return Detail::DownloadFailure("下载文件名、摘要或票据不匹配，发布可能已变更，请重新检查版本");
            cloud.file_id = file_id; // 后端取链接响应没有 file_id，使用本次已绑定请求 ID
            Cloud_Download_Snapshot download;
            download.file_id = file_id; download.file_name = cloud.file_name;
            download.file_md5 = md5; download.ticket = cloud.ticket;
            download.source = cloud.source; download.download_url = cloud.download_url;
            download.file_size = cloud.file_size;
            download.device_id = runtime.device_id; download.app_id = runtime.app.App_Pid;
            download.developer_uid = runtime.account.Account_Uid; download.api_url = runtime.api_url;
            download.card_key = runtime.card_key; download.player_uid = runtime.player_uid;
            download.player_name = runtime.player_name; download.generation = runtime.generation;
            Detail::Publish(result);
            return {true, result.message, cloud, download};
        }

        Cloud_Download_Result GetDownloadUrlSync(const std::string& file_name)
        {
            std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
            if (file_name.empty()) return Detail::DownloadFailure("文件名不能为空");
            const auto runtime = JiYanSDK::GetRuntimeSnapshot();
            auto check = Detail::Request("check_version", {{"Name", file_name}});
            if (!check.request_ok) return {false, check.message, check.cloud, {}};
            if (!check.cloud.has_file || check.cloud.file_name != file_name ||
                !Detail::ValidId(check.cloud.file_id) || Detail::LowerMD5(check.cloud.latest_md5).empty())
                return Detail::DownloadFailure("没有同名且具有效 MD5 的已发布文件，请重新检查版本");
            if (!Detail::SameRuntime(runtime, JiYanSDK::GetRuntimeSnapshot()))
                return Detail::DownloadFailure("检查期间身份、设备或应用已变更，请重新检查版本");
            return GetDownloadUrlById(check.cloud.file_id, file_name, check.cloud.latest_md5);
        }

        Cloud_Update_Result ConfirmDownload(const Cloud_Download_Snapshot& download, const std::string& local_file_path)
        {
            std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
            if (!Detail::Matches(download, JiYanSDK::GetRuntimeSnapshot()))
                return Detail::Failure("下载身份、设备或应用已变更，不能确认旧票据");
            const auto expected = Detail::LowerMD5(download.file_md5);
            if (!Detail::ValidId(download.file_id) || expected.empty() ||
                (download.source != "local" && download.source != "remote") ||
                (download.source == "local" && download.ticket.empty()))
                return Detail::Failure("下载快照无效或缺少预期 MD5/票据，不能回执");
            const auto actual = JiYanSDK_Function::Get_File_MD5(local_file_path);
            if (actual.empty()) return Detail::Failure("下载文件读取失败，未提交回执");
            if (actual != expected) return Detail::Failure("下载文件 MD5 不匹配，未提交回执");
            if (!Detail::Matches(download, JiYanSDK::GetRuntimeSnapshot()))
                return Detail::Failure("校验期间身份、设备或应用已变更，未提交回执");
            auto result = Detail::Request("confirm_download", {{"file_id", download.file_id},
                {"ticket", download.ticket}, {"device_id", download.device_id}, {"md5_ok", 1}});
            if (!Detail::Matches(download, JiYanSDK::GetRuntimeSnapshot()))
                return Detail::Failure("回执期间身份、设备或应用已变更，拒绝应用旧响应");
            if (result.request_ok && result.cloud.confirmed != 1)
                return Detail::Failure("回执响应未确认下载完成");
            return result;
        }

        namespace Detail
        {
            // 兼容旧异步习惯；新接口应直接保存返回值，避免共享结果被覆盖。
            void Legacy(std::function<Cloud_Update_Result()> task, bool need_update_result,
                        bool& value, std::string& message, std::function<void()> callback)
            {
                if (is_requesting.exchange(true)) {
                    value = false;
                    message = "已有异步请求正在进行，请稍后重试";
                    if (callback) callback();
                    return;
                }
                value = false; message = "wait...";
                auto run = [task, need_update_result, &value, &message, callback]() {
                    struct Finally { ~Finally() { is_requesting.store(false); } } finally;
                    try {
                        const auto result = task();
                        value = result.request_ok && (!need_update_result || result.cloud.need_update == 1);
                        message = result.message;
                    } catch (const std::exception& e) {
                        std::lock_guard<std::recursive_mutex> lock(JiYanSDK::RequestMutex());
                        const auto result = Failure(std::string("云更新请求异常：") + e.what());
                        value = false; message = result.message;
                    }
                    if (JiYanSDK::Request_Completed_Callback) JiYanSDK::Request_Completed_Callback();
                    if (callback) callback();
                };
                if (JiYanSDK::bUseSingleThreaded) run();
                else std::thread(run).detach();
            }
        }

        // 旧 bool 含义是 need_update；失败时 false，不读取上次 cloud.need_update。
        void CheckVersion(std::string so_Name, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            Detail::Legacy([so_Name]() { return CheckVersionSync(so_Name); }, true, is_success, result, callback);
        }

        void CheckVersionByVersion(std::string version, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            Detail::Legacy([version]() { return CheckVersionByVersionSync(version); }, true, is_success, result, callback);
        }

        void GetFileList(bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            RejectManagement(is_success, result, callback);
        }

        void GetDownloadUrl(std::string file_name, bool& is_success, std::string& result, std::function<void()> callback = nullptr)
        {
            Detail::Legacy([file_name]() {
                const auto download = GetDownloadUrlSync(file_name);
                return Cloud_Update_Result{download.request_ok, download.message, download.cloud};
            }, false, is_success, result, callback);
        }
    }
}

std::string 获取公告()
{
    // 请求内部持锁，局部data独立；完成回调必须能安全停止心跳或发起下一次验证。
    JiYanSDK::set_php("key.php");
    bool is_success = false;
    std::string result;
    nlohmann::json data;

    JiYanSDK_API::start("get_announcement", nlohmann::json::object(), is_success, result, data);

    if (is_success && data.is_object() && data.contains("announcement") && data["announcement"].is_string())
    {
        return data["announcement"].get<std::string>();
    }
    return result;
}

#include "SDK_Heartbeat.cpp"

#endif