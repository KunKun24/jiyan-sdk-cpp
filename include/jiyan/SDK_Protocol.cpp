/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_PROTOCOL_CPP
#define JIYAN_SDK_PROTOCOL_CPP

#include <map>
#include <set>
#include <stdexcept>

namespace JiYanProtocol {
using Json = nlohmann::json;

struct Request {
    Json outer;
    std::string rc, nonce, iv;
};
struct Response {
    bool ok = false;
    int code = -1;
    std::string message, plain;
    Json data = Json::object();
};

inline std::string Scalar(const Json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_boolean()) return value.get<bool>() ? "1" : "";
    if (value.is_null()) return "";
    if (value.is_number_integer() || value.is_number_unsigned()) return value.dump();
    throw std::runtime_error("请求参数必须是字符串或整数，数组应先编码成JSON字符串");
}
inline std::string Canonical(const Json& fields, const std::string& app_id, const std::string& scope) {
    std::map<std::string, std::string> sorted;
    for (const auto& field : fields.items()) if (field.key() != "signature") sorted[field.key()] = Scalar(field.value());
    std::string out = "jy-req-v2\n" + scope + "\n" + app_id + "\n2\n";
    for (const auto& field : sorted)
        out += std::to_string(field.first.size()) + ":" + field.first + "=" +
               std::to_string(field.second.size()) + ":" + field.second + "\n";
    return out;
}
inline Request Encode(const JiYanSDK::RuntimeSnapshot& snapshot, const std::string& action,
                      Json fields, const std::string& scope) {
    const auto& app = snapshot.app;
    if (app.Crypto_Method < 0 || app.Crypto_Method > 8 || app.Signature_Method < 0 || app.Signature_Method > 14)
        throw std::runtime_error("加密或签名方式编号非法");
    if (app.Signature_Method != 0 && app.Signature_Key.empty()) throw std::runtime_error("Signature_Key 未配置");
    if ((app.Crypto_Method == 1 || app.Crypto_Method == 4 || app.Crypto_Method == 5 ||
         app.Crypto_Method == 6 || app.Crypto_Method == 7 || app.Crypto_Method == 8) && app.Crypto_Key.empty())
        throw std::runtime_error("Crypto_Key 未配置");
    if (!fields.is_object()) fields = Json::object();
    Request result;
    result.rc = JiYanSDK_Function::Random_String(24);
    result.nonce = JiYanSDK_Function::Random_String(24);
    fields["action"] = action;
    fields["app_id"] = app.App_Pid;
    fields["signature_version"] = "2";
    fields["signature_scope"] = scope;
    fields["timestamp"] = JiYanSDK_Function::Get_Timestamp();
    fields["nonce"] = result.nonce;
    fields["rc"] = result.rc;
    fields.erase("signature");
    const auto signature = JiYan_Signature_Content(Canonical(fields, app.App_Pid, scope), app);
    if (app.Signature_Method != 0 && signature.empty()) throw std::runtime_error("本地签名失败");
    fields["signature"] = signature;
    std::string query;
    for (const auto& field : fields.items()) {
        if (!query.empty()) query += '&';
        query += JiYanSDK_Function::Url_Encode(field.key()) + "=" + JiYanSDK_Function::Url_Encode(Scalar(field.value()));
    }
    const Json encrypted = JiYan_Crypto_Encryption_Content(query, snapshot);
    const auto cipher = encrypted.value("密文", std::string());
    result.iv = encrypted.value("IV", std::string());
    if (cipher.empty()) throw std::runtime_error("本地加密失败，请检查 Crypto_Key 与 Crypto_Method");
    result.outer = {{"developer_username", snapshot.account.Account_Uid}, {"app_id", app.App_Pid},
                    {"iv", result.iv}, {"data", cipher}};
    return result;
}

// data必须使用线上原字节验签，不能parse后重新dump改变字段顺序或转义。
inline Response Decode(const JiYanSDK::RuntimeSnapshot& snapshot, const Request& request,
                       const std::string& wire, bool cloud_control = false) {
    Response out;
    if (wire.rfind("JiYan_Error", 0) == 0) {
        out.message = wire.substr(11); out.code = 9999; return out;
    }
    out.plain = JiYan_Crypto_Decryption_Content(wire, snapshot, request.iv);
    if (out.plain.empty()) { out.code = 1002; out.message = "响应解密失败（Crypto_Key / Crypto_Method 配置不匹配）"; return out; }

    std::vector<std::set<std::string>> objects;
    bool duplicate = false;
    auto callback = [&](int, Json::parse_event_t event, Json& parsed) {
        if (event == Json::parse_event_t::object_start) objects.emplace_back();
        else if (event == Json::parse_event_t::key && !objects.back().insert(parsed.get<std::string>()).second) duplicate = true;
        else if (event == Json::parse_event_t::object_end) objects.pop_back();
        return true;
    };
    const auto root = Json::parse(out.plain, callback);
    if (duplicate || !root.is_object() || !root.contains("success") || !root["success"].is_boolean() ||
        !root.contains("message") || !root["message"].is_string() || !root.contains("data"))
        throw std::runtime_error("响应结构非法");
    const bool success = root["success"].get<bool>();
    if (snapshot.app.Signature_Method != 0 && !VerifyResponseSignature(root, out.plain, snapshot.app)) {
        out.code = 1003; out.message = "响应签名校验失败"; return out;
    }
    const auto& data = root["data"];

    if (success || cloud_control || (data.is_object() && data.contains("rc"))) {
        if (!data.is_object() || data.value("rc", std::string()) != request.rc) {
            out.code = 1003; out.message = "响应校验失败（rc 不匹配或缺失）"; return out;
        }
    }
    if (cloud_control && (data.value("request_nonce", std::string()) != request.nonce ||
        data.value("signature_scope", std::string()) != "cloud_control" ||
        data.value("app_id", std::string()) != snapshot.app.App_Pid ||
        data.value("card_key", std::string()) != snapshot.card_key ||
        data.value("device_id", std::string()) != snapshot.device_id)) {
        out.code = 1003; out.message = "云控响应身份绑定不匹配"; return out;
    }

    out.ok = success;
    out.code = success ? 0 : root.value("code", 9999);
    if (!success && out.code == 0) out.code = 9999;
    out.message = root["message"].get<std::string>();
    out.data = data;
    return out;
}

inline Response Exchange(const JiYanSDK::RuntimeSnapshot& snapshot, const std::string& endpoint,
                         const std::string& action, Json fields, const std::string& scope,
                         bool cloud_control = false) {
    const auto request = Encode(snapshot, action, std::move(fields), scope);
    std::string wire, error;
    int status = 0;
    if (!JiYanNet::http_post(snapshot.api_url + endpoint, request.outer.dump(), "application/json", wire, error, status)) {
        Response result; result.message = error.empty() ? "网络请求失败" : error; return result;
    }
    return Decode(snapshot, request, wire, cloud_control);
}
}
#endif
