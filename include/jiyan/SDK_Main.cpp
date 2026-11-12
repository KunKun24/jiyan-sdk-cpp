/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */
#ifndef JIYAN_SDK_MAIN_CPP
#define JIYAN_SDK_MAIN_CPP

#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>

#include "json.hpp"

#include "SDK_Result.cpp"
#include "SDK_Init.cpp"

#include "SDK_TLS/JiYan_Signature.cpp"

#include "SDK_Function.cpp"

#include "SDK_TLS/JiYan_Crypto.cpp"

#include <algorithm>
#include "SDK_Http.cpp"

#define JiYan_Cloud_Version "1.8"

thread_local bool JiYan_Last_Request_Ok = false;

using JiYanNet::http_post;

std::string GenerateRandomCode() {
    return JiYanSDK_Function::Random_String(12);
}

std::string BuildSignatureString(const std::string& action, const std::string& rc, const nlohmann::json& params = {}) {
    std::string result;

    if (!action.empty()) {
        result = action;
    }

    for (const auto& [key, value] : params.items()) {
        if (key == "signature") {
            continue;
        }
        if (value.is_string()) {
            result += value.get<std::string>();
        } else {
            result += value.dump();
        }
    }

    result += rc;

    return result;
}

std::string RawValueOf(const std::string& jsonText, const std::string& key) {
    const std::string pat = "\"" + key + "\"";
    int depth = 0;
    bool inStr = false;
    bool esc = false;

    for (size_t i = 0; i < jsonText.size(); ++i) {
        const char c = jsonText[i];
        if (inStr) {
            if (esc) { esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') {
            if (depth == 1 && jsonText.compare(i, pat.size(), pat) == 0) {
                size_t q = i + pat.size();
                while (q < jsonText.size() && (jsonText[q] == ' ' || jsonText[q] == '\n' ||
                                               jsonText[q] == '\r' || jsonText[q] == '\t')) ++q;
                if (q < jsonText.size() && jsonText[q] == ':') {
                    ++q;
                    while (q < jsonText.size() && (jsonText[q] == ' ' || jsonText[q] == '\n' ||
                                                   jsonText[q] == '\r' || jsonText[q] == '\t')) ++q;
                    if (q >= jsonText.size()) return std::string();

                    const size_t start = q;
                    if (jsonText[q] == '{' || jsonText[q] == '[') {
                        const char open = jsonText[q];
                        const char close = (open == '{') ? '}' : ']';
                        int d = 0;
                        bool s2 = false, e2 = false;
                        for (size_t k = q; k < jsonText.size(); ++k) {
                            const char ck = jsonText[k];
                            if (s2) {
                                if (e2) { e2 = false; continue; }
                                if (ck == '\\') { e2 = true; continue; }
                                if (ck == '"') s2 = false;
                                continue;
                            }
                            if (ck == '"') { s2 = true; continue; }
                            if (ck == open) ++d;
                            else if (ck == close) {
                                --d;
                                if (d == 0) return jsonText.substr(start, k - start + 1);
                            }
                        }
                        return std::string();
                    }
                    if (jsonText[q] == '"') {
                        bool e3 = false;
                        size_t k = q + 1;
                        for (; k < jsonText.size(); ++k) {
                            const char ck = jsonText[k];
                            if (e3) { e3 = false; continue; }
                            if (ck == '\\') { e3 = true; continue; }
                            if (ck == '"') { ++k; break; }
                        }
                        return jsonText.substr(start, k - start);
                    }
                    size_t k = q;
                    while (k < jsonText.size() && jsonText[k] != ',' && jsonText[k] != '}' &&
                           jsonText[k] != ']' && jsonText[k] != '\n' && jsonText[k] != ' ')
                        ++k;
                    return jsonText.substr(start, k - start);
                }
                inStr = true;
                esc = false;
                continue;
            }
            inStr = true;
            esc = false;
            continue;
        }
        if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') --depth;
    }
    return std::string();
}

bool VerifyResponseSignature(const nlohmann::json& response, const std::string& plainText, const App_Info& app) {
    try {
        if (!response.contains("success") || !response.contains("signature")) {
            return false;
        }

        const bool success = response["success"].get<bool>();
        const std::string message = response.contains("message") && response["message"].is_string()
                                        ? response["message"].get<std::string>()
                                        : std::string();

        const std::string dataRaw = RawValueOf(plainText, "data");
        if (dataRaw.empty()) return false; // 不能把提取失败当null验签后采用另一个data。

        const std::string content = (success ? std::string("true") : std::string("false")) + message + dataRaw;
        const std::string calculated = JiYan_Signature_Content(content, app);
        const std::string original = response["signature"].get<std::string>();

        return JiYan_Secure_Equal(calculated, original);
    } catch (const std::exception& e) {
        return false;
    }
}

void FillResultFromData(const nlohmann::json& data) {
    Result_Data& R = JiYanSDK_Result::Data;

    if (!data.is_object()) {
        return;
    }

    if (data.contains("到期时间") && data["到期时间"].is_string()) {
        R.card.expire_text = data["到期时间"].get<std::string>();
    }
    if (data.contains("剩余秒")) {
        if (data["剩余秒"].is_number()) R.card.seconds_left = data["剩余秒"].get<long long>();
        else if (data["剩余秒"].is_string()) R.card.seconds_left = atoll(data["剩余秒"].get<std::string>().c_str());
    }
    if (data.contains("type")) {
        if (data["type"].is_string()) R.card.type = data["type"].get<std::string>();
        else if (data["type"].is_number()) R.card.type = std::to_string(data["type"].get<int>());
    }
    if (data.contains("need_reverify") && data["need_reverify"].is_number()) {
        R.card.need_reverify = data["need_reverify"].get<int>();
    }

    if (data.contains("在线人数") && data["在线人数"].is_number()) {
        R.online.count = data["在线人数"].get<int>();
        JiYanSDK_Result::onlineCount = R.online.count;
    }
    if (data.contains("is_peer") && data["is_peer"].is_number()) {
        R.online.is_peer = data["is_peer"].get<int>();
    }
    if (data.contains("server_time") && data["server_time"].is_number()) {
        R.online.server_time = data["server_time"].get<int>();
    }
    if (data.contains("game_uids") && data["game_uids"].is_array()) {
        R.online.game_uids.clear();
        for (const auto& uid : data["game_uids"]) {
            if (uid.is_string()) R.online.game_uids.push_back(uid.get<std::string>());
        }
    }
    if (data.contains("online_list") && data["online_list"].is_array()) {
        R.online.list.clear();
        for (const auto& item : data["online_list"]) {
            if (!item.is_object()) continue;
            Peer_Info p;
            if (item.contains("user_name") && item["user_name"].is_string()) p.user_name = item["user_name"].get<std::string>();
            if (item.contains("card_no") && item["card_no"].is_string()) p.card_no = item["card_no"].get<std::string>();
            if (item.contains("device_id") && item["device_id"].is_string()) p.device_id = item["device_id"].get<std::string>();
            if (item.contains("last_seen") && item["last_seen"].is_string()) p.last_seen = item["last_seen"].get<std::string>();
            if (item.contains("last_seen_ts") && item["last_seen_ts"].is_number()) p.last_seen_ts = item["last_seen_ts"].get<long long>();
            if (item.contains("is_self") && item["is_self"].is_number()) p.is_self = item["is_self"].get<int>();
            R.online.list.push_back(p);
        }
    }

    if (data.contains("心跳间隔秒") && data["心跳间隔秒"].is_number()) {
        R.session.heartbeat_seconds = data["心跳间隔秒"].get<int>();
    }
    if (data.contains("心跳间隔") && data["心跳间隔"].is_number()) {
        R.session.heartbeat_interval = data["心跳间隔"].get<int>();
    }
    if (data.contains("rc") && data["rc"].is_string()) {
        R.session.rc = data["rc"].get<std::string>();
    }

    if (data.contains("need_update")) {
        if (data["need_update"].is_number()) R.cloud.need_update = data["need_update"].get<int>();
        else if (data["need_update"].is_boolean()) R.cloud.need_update = data["need_update"].get<bool>() ? 1 : 0;
    }
    if (data.contains("has_file")) {
        if (data["has_file"].is_number()) R.cloud.has_file = data["has_file"].get<int>();
        else if (data["has_file"].is_boolean()) R.cloud.has_file = data["has_file"].get<bool>() ? 1 : 0;
    }
    if (data.contains("file_id")) {
        if (data["file_id"].is_string()) R.cloud.file_id = data["file_id"].get<std::string>();
        else if (data["file_id"].is_number()) R.cloud.file_id = std::to_string(data["file_id"].get<long long>());
    }
    if (data.contains("file_name") && data["file_name"].is_string()) {
        R.cloud.file_name = data["file_name"].get<std::string>();
    }
    if (data.contains("file_size") && data["file_size"].is_number()) {
        R.cloud.file_size = data["file_size"].get<long long>();
    }
    if (data.contains("latest_md5") && data["latest_md5"].is_string()) {
        R.cloud.latest_md5 = data["latest_md5"].get<std::string>();
    }
    if (data.contains("version")) {
        if (data["version"].is_string()) R.cloud.version = data["version"].get<std::string>();
        else if (data["version"].is_number()) R.cloud.version = std::to_string(data["version"].get<long long>());
    }
    if (data.contains("download_url") && data["download_url"].is_string()) {
        R.cloud.download_url = data["download_url"].get<std::string>();
    }
    for (const auto& field : {"ticket", "source", "file_md5"}) {
        if (!data.contains(field) || !data[field].is_string()) continue;
        if (std::string(field) == "ticket") R.cloud.ticket = data[field].get<std::string>();
        else if (std::string(field) == "source") R.cloud.source = data[field].get<std::string>();
        else R.cloud.file_md5 = data[field].get<std::string>();
    }
    if (data.contains("confirmed") && data["confirmed"].is_number_integer()) R.cloud.confirmed = data["confirmed"].get<int>();
    if (data.contains("charged") && data["charged"].is_number_integer()) R.cloud.charged = data["charged"].get<int>();
    if (data.contains("remaining_times") && data["remaining_times"].is_number_integer()) R.cloud.remaining_times = data["remaining_times"].get<long long>();
    if (data.contains("update_content") && data["update_content"].is_string()) {
        R.cloud.update_content = data["update_content"].get<std::string>();
    } else if (data.contains("changelog") && data["changelog"].is_string()) {
        R.cloud.update_content = data["changelog"].get<std::string>();
    }
}

#include "SDK_Protocol.cpp"

std::string API_Request(const std::string& action, const nlohmann::json& params = {}) {
    std::lock_guard<std::recursive_mutex> request_lock(JiYanSDK::RequestMutex());
    JiYan_Last_Request_Ok = false;
    auto& result = JiYanSDK_Result::Data;
    result.ok = false; result.code = 0; result.message.clear(); result.raw.clear();
    result.cloud = Cloud_Info{};
    try {
        const auto snapshot = JiYanSDK::GetRuntimeSnapshot();
        const auto endpoint = JiYanSDK::php_action;
        std::string scope;
        if (endpoint == "key.php") scope = "key";
        else if (endpoint == "user.php") scope = "user";
        else if (endpoint == "Cloudupdate.php") scope = "cloud";
        else throw std::runtime_error("此客户端只允许卡密、同行识别和云更新接口");
        auto response = JiYanProtocol::Exchange(snapshot, endpoint, action, params, scope);
        result.code = response.code;
        result.message = response.message;
        result.raw = response.plain;
        result.ok = response.ok;
        if (response.ok) FillResultFromData(response.data);
        result.cloud.request_ok = response.ok;
        JiYan_Last_Request_Ok = response.ok;
        return response.plain.empty() ? response.message : response.plain;
    } catch (const std::exception& failure) {
        result.ok = false; result.code = 9999; result.message = failure.what();
        return result.message;
    }
}

#include "SDK_CloudControl.cpp"
#include "SDK_Api.cpp"
#include "SDK_Chinese.cpp"

#endif