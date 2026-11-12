/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_INIT_CPP
#define JIYAN_SDK_INIT_CPP

#include <jni.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <functional>

struct Account_Info {
    std::string Account_Uid;
    std::string Account_Pass;
    std::string Account_Token;
};
struct App_Info {
    std::string App_Pid;
    int Crypto_Method;
    std::string Crypto_Key;
    int Signature_Method;
    std::string Signature_Key;
};

namespace JiYanSDK {
    inline std::atomic<JavaVM*>& JavaVMStorage() {
        static std::atomic<JavaVM*> vm{nullptr};
        return vm;
    }

    inline nlohmann::json& ExtraParams() { static nlohmann::json v = nlohmann::json::object(); return v; }
    inline bool& TimestampCheckEnabled() { static bool v = true; return v; }
    inline std::function<void()>& GlobalResponseHandler() { static std::function<void()> fn; return fn; }

    inline bool set_java_vm(JavaVM* vm) {
        if (!vm) return false;
        JavaVM* expected = nullptr;
        return JavaVMStorage().compare_exchange_strong(expected, vm) || expected == vm;
    }
    inline JavaVM* get_java_vm() { return JavaVMStorage().load(); }

    std::string api_url = "http://api.xn--0try77c6um82w.cn/api/";
    thread_local std::string php_action;
    Account_Info account_info;
    App_Info app_info{};
    std::string Device_Id;
    bool bUseSingleThreaded = false;
    std::atomic<bool> bLocked{false};
    std::function<void()> Request_Completed_Callback;

    struct RuntimeSnapshot {
        Account_Info account;
        App_Info app{};
        std::string api_url, device_id, card_key, player_uid, player_name;
        bool verified = false;
        bool identity_ready = true;
        std::uint64_t generation = 1;
    };
    inline std::mutex& RuntimeMutex() { static std::mutex m; return m; }
    inline RuntimeSnapshot& Runtime() { static RuntimeSnapshot s; return s; }
    inline std::recursive_mutex& RequestMutex() { static std::recursive_mutex m; return m; }
    inline RuntimeSnapshot GetRuntimeSnapshot() {
        std::lock_guard<std::mutex> lock(RuntimeMutex());
        RuntimeSnapshot s = Runtime();
        s.account = account_info; s.app = app_info; s.api_url = api_url; s.device_id = Device_Id;
        return s;
    }
    void ClearVerifiedCard() {
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        auto& s = Runtime();
        if (s.verified || !s.card_key.empty()) { s.verified = false; s.card_key.clear(); ++s.generation; }
    }
    void SetVerifiedCard(const std::string& key) {
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        auto& s = Runtime();
        if (!s.verified || s.card_key != key) ++s.generation;
        s.card_key = key; s.verified = !key.empty();
    }
    bool SetVerifiedCardIfCurrent(const std::string& key, std::uint64_t generation) {
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        auto& s = Runtime();
        if (s.generation != generation) return false;
        if (!s.verified || s.card_key != key) ++s.generation;
        s.card_key = key; s.verified = !key.empty();
        return true;
    }
    // 只缓存身份；唯一网络上报仍由 Gameinfo::Refresh / 卡密验证 / 心跳完成。
    bool SetPlayerInfo(const std::string& name, const std::string& uid) {
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        auto& s = Runtime();
        const bool changed = s.player_uid != uid || s.player_name != name;
        if (s.player_uid != uid) { ++s.generation; s.identity_ready = false; }
        s.player_uid = uid; s.player_name = name;
        return changed;
    }
    void MarkIdentityUploaded(std::uint64_t generation) {
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        if (Runtime().generation == generation &&
            (!Runtime().player_uid.empty() || Runtime().identity_ready)) Runtime().identity_ready = true;
    }
    void init(Account_Info accountinfo, App_Info appinfo) {
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        account_info = accountinfo; app_info = appinfo;
        auto& s = Runtime(); s.verified = false; s.card_key.clear(); ++s.generation;
        bLocked.store(false);
    }
    void set_device_id(std::string id) {
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        if (Device_Id != id) { Runtime().verified = false; Runtime().card_key.clear(); ++Runtime().generation; }
        Device_Id = id;
    }
    void Enable_SingleThreaded() { bUseSingleThreaded = true; }
    void Close_SingleThreaded() { bUseSingleThreaded = false; }
    void set_php(std::string name) { php_action = name; }
    void set_api_url(std::string url) {
        while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
        if (url.empty()) url = "http://api.xn--0try77c6um82w.cn/api";
        if (url.find("://") == std::string::npos) url = "http://" + url;
        const auto authority = url.find("://") + 3;
        if (url.find('/', authority) == std::string::npos) url += "/api";
        // 显式 https 保留原样，由 HTTP 模块明确拒绝，绝不偷偷降级。
                std::lock_guard<std::mutex> lock(RuntimeMutex());
        if (api_url != url + "/") ++Runtime().generation;
        api_url = url + "/";
    }
    void Set_Request_Completed_Callback(std::function<void()> fn) { Request_Completed_Callback = fn; }
    bool is_locked() { return bLocked.load(); }
    void set_locked(bool value) { bLocked.store(value); if (value) ClearVerifiedCard(); }
    void Reset_Lock() { bLocked.store(false); }
}
#endif
