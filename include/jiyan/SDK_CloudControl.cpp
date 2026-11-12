/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_CLOUD_CONTROL_CPP
#define JIYAN_SDK_CLOUD_CONTROL_CPP

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class RemoteConfig {
public:
    static RemoteConfig& GetInstance() {
        static RemoteConfig instance;
        return instance;
    }

    bool BindValue(const std::string& key, bool* value) { return RegisterValue(key, value, Kind::Boolean); }
    bool BindValue(const std::string& key, int* value) { return RegisterValue(key, value, Kind::Integer); }
    bool BindValue(const std::string& key, float* value) { return RegisterValue(key, value, Kind::Float); }
    // 文本类功能（下拉框、枚举、字符串开关）。远程值必须是字符串。
    bool BindValue(const std::string& key, std::string* value) { return RegisterValue(key, value, Kind::String); }

    // 下拉框：服务端存字符串，本地用 ImGui::Combo 需要的 int 索引。
    bool BindSelect(const std::string& key, int* index, const std::vector<std::string>& options) {
        if (!index) return Fail("binding address is null");
        if (options.empty() || options.size() > 100) return Fail("select options must contain 1..100 entries");
        std::set<std::string> unique;
        for (const auto& option : options) {
            if (option.empty() || option.size() > 64)
                return Fail("invalid select option (1..64 bytes): " + option);
            if (!unique.insert(option).second) return Fail("duplicate select option: " + option);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ValidKey(key)) return FailLocked("invalid feature key (use [A-Za-z][A-Za-z0-9_]{0,63})");
        const auto existing = bindings_.find(key);
        if (existing != bindings_.end()) {
            if (existing->second.address == index && existing->second.kind == Kind::Select &&
                existing->second.options == options) return true;
            return FailLocked("feature key is already bound: " + key);
        }
        for (const auto& item : bindings_)
            if (item.second.address == index) return FailLocked("address is already bound to: " + item.first);
        if (bindings_.size() >= MaxBindings) return FailLocked("at most 100 features may be bound");
        Binding binding;
        binding.kind = Kind::Select;
        binding.address = index;
        binding.options = options;
        bindings_.emplace(key, std::move(binding));
        changed_.notify_all();
        return true;
    }

    // 请求下一轮立即同步，不必等到 poll_after。用于用户手动点“立即同步”或面板操作后加速。
    void RequestSync() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            force_sync_ = true;
        }
        changed_.notify_all();
    }

    // 服务端建议的轮询间隔（秒），由最近一次成功响应给出，默认 2 秒。
    int PollIntervalSeconds() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return poll_after_;
    }

    // 最近一次失败原因；空串表示上次同步正常。可用在设置界面直接展示。
    std::string LastError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }

    template<class Callback>
    bool BindButton(const std::string& key, Callback&& callback) {
        using Fn = typename std::decay<Callback>::type;
        using Return = typename std::invoke_result<Fn&>::type;
        static_assert(std::is_same<Return, bool>::value || std::is_same<Return, void>::value,
                      "BindButton callback must return bool or void");
        Fn fn(std::forward<Callback>(callback));
        if constexpr (std::is_same<Fn, std::function<bool()>>::value ||
                      std::is_same<Fn, std::function<void()>>::value || std::is_pointer<Fn>::value) {
            if (!fn) return Fail("button callback is empty");
        }
        Binding binding;
        binding.kind = Kind::Button;
        binding.callback = [fn = std::move(fn)]() mutable -> bool {
            if constexpr (std::is_same<Return, bool>::value) return fn();
            else { fn(); return true; }
        };
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ValidKey(key)) return FailLocked("invalid feature key (use [A-Za-z][A-Za-z0-9_]{0,63})");
        if (bindings_.count(key)) return FailLocked("feature key is already bound: " + key);
        if (bindings_.size() >= MaxBindings) return FailLocked("at most 100 features may be bound");
        bindings_.emplace(key, std::move(binding));
        changed_.notify_all();
        return true;
    }

    // 首次有绑定的 Update 自动启动；没有绑定或没有已验证身份时不会联网。
    void Update() {
        const auto snapshot = JiYanSDK::GetRuntimeSnapshot();
        std::vector<Task> tasks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (updating_) return; // 回调重入 Update 时不重复执行命令。
            updating_ = true;
            try {
                ObserveIdentityLocked(snapshot);
                if (restore_pending_) {
                    for (auto& item : bindings_) {
                        RestoreLocal(item.second);
                        item.second.default_initialized = false;
                    }
                    restore_pending_ = false;
                }
                if ((!snapshot.verified || !snapshot.identity_ready) || stopped_) {
                    updating_ = false;
                    return;
                }
                const auto now = Clock::now();
                for (auto& item : bindings_) {
                    Binding& binding = item.second;
                    if (binding.kind == Kind::Button) continue;
                    const auto remote = remote_values_.find(item.first);
                    if (remote == remote_values_.end()) {
                        RestoreLocal(binding);
                        binding.default_initialized = false;
                        continue;
                    }
                    const RemoteValue& value = remote->second;
                    if (value.source == "default") {
                        if (binding.active) ReleaseDefault(binding, value.value);
                        else if (!binding.default_initialized) WriteValue(binding, value.value);
                        binding.default_initialized = true;
                    } else if (value.deadline <= now) {
                        if (binding.active) ReleaseDefault(binding, value.default_value);
                        // 即使主线程错过了覆盖生效期，也只应用一次回落默认值。
                        else if (!binding.default_initialized) WriteValue(binding, value.default_value);
                        binding.default_initialized = true;
                    } else {
                        if (!binding.active) {
                            binding.local_before = ReadValue(binding);
                            binding.active = true;
                        }
                        WriteValue(binding, value.value); // global/user 每帧强制覆盖。
                        binding.default_initialized = false;
                    }
                }
                PruneSeenLocked(now);
                for (auto& item : pending_commands_) {
                    Command& command = item.second;
                    if (command.started) continue;
                    // ACK 队列满时保留命令，等待下次服务器确认后再处理。
                    if (acks_.size() >= MaxAcks || seen_commands_.size() >= MaxSeen) break;
                    command.started = true; // 必须在释放锁、运行用户回调之前登记。
                    const auto base = std::max(command.deadline, now);
                    const auto retention = std::chrono::seconds(60);
                    const auto remembered_until = TimePoint::max() - base <= retention ?
                        TimePoint::max() : base + retention;
                    seen_commands_.emplace(command.id_key, remembered_until);
                    Ack ack;
                    ack.id = command.id;
                    ack.status = "failed";
                    ack.message = kAckPending;
                    acks_.emplace(command.id_key, std::move(ack));
                    if (command.deadline <= now) {
                        SetAckLocked(command.id_key, "failed", kAckExpired);
                        continue;
                    }
                    const auto binding = bindings_.find(command.key);
                    if (binding == bindings_.end() || binding->second.kind != Kind::Button) {
                        SetAckLocked(command.id_key, "failed", kAckNotBound);
                        continue;
                    }
                    tasks.push_back(Task{command.id_key, command.deadline, identity_epoch_,
                                         binding->second.callback});
                }
                for (auto it = pending_commands_.begin(); it != pending_commands_.end();) {
                    if (it->second.started) it = pending_commands_.erase(it);
                    else ++it;
                }
                if (!worker_.joinable() && !bindings_.empty()) {
                    stop_ = false;
                    worker_ = std::thread(&RemoteConfig::Worker, this);
                }
            } catch (const std::exception& error) {
                last_error_ = std::string("cloud Update failed: ") + error.what();
                updating_ = false;
                return;
            } catch (...) {
                last_error_ = "cloud Update failed";
                updating_ = false;
                return;
            }
        }
        for (auto& task : tasks) {
            // 前一个回调可能更换卡密、注销、Shutdown；下一条旧身份命令不得执行。
            const auto current = JiYanSDK::GetRuntimeSnapshot();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ObserveIdentityLocked(current);
                if (task.epoch != identity_epoch_ || stopped_ || !current.verified || !current.identity_ready) continue;
                if (task.deadline <= Clock::now()) {
                    SetAckLocked(task.id_key, "failed", kAckExpired);
                    continue;
                }
            }
            bool success = false;
            std::string message;
            try {
                success = task.callback();
                message = success ? kAckDone : kAckFalse;
            } catch (const std::exception& error) {
                message = std::string("回调异常：") + error.what();
            } catch (...) {
                message = "回调异常";
            }
            const auto after_callback = JiYanSDK::GetRuntimeSnapshot();
            std::lock_guard<std::mutex> lock(mutex_);
            ObserveIdentityLocked(after_callback);
            if (task.epoch == identity_epoch_ && !stopped_)
                SetAckLocked(task.id_key, success ? "success" : "failed", message);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        updating_ = false;
    }

    bool IsRemoteOverride(const std::string& key) const {
        const auto snapshot = JiYanSDK::GetRuntimeSnapshot();
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_ || !have_identity_ || !SameIdentity(snapshot, identity_) || (!snapshot.verified || !snapshot.identity_ready)) return false;
        const auto found = remote_values_.find(key);
        return found != remote_values_.end() && found->second.source != "default" &&
               found->second.deadline > Clock::now();
    }

    std::string GetLastError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }

    // 显式主线程退出：先恢复所有活跃覆盖，再停止并等待网络线程退出。
    // Shutdown 是终止操作；保留登记，后续 Update 不会重新启动网络。
    void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
            stop_ = true;
            for (auto& item : bindings_) RestoreLocal(item.second);
            ClearRemoteLocked();
            restore_pending_ = false;
            changed_.notify_all();
        }
        if (worker_.joinable()) worker_.join();
    }

    RemoteConfig(const RemoteConfig&) = delete;
    RemoteConfig& operator=(const RemoteConfig&) = delete;

private:
    using Json = nlohmann::json;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    enum class Kind { Boolean, Integer, Float, Button, String, Select };
    static constexpr std::size_t MaxBindings = 100;
    static constexpr std::size_t MaxAcks = 100;
    static constexpr std::size_t MaxSeen = 4096;
    // ACK 文案：直接显示在云控面板「成功 / 失败」详情里，必须是中文。
    // kAckPending 只作为本进程内部占位标记，永远不会发送给服务端。
    static constexpr const char* kAckPending = "callback pending";
    static constexpr const char* kAckExpired = "指令已过期，未执行";
    static constexpr const char* kAckNotBound = "客户端未绑定该按钮";
    static constexpr const char* kAckDone = "执行成功";
    static constexpr const char* kAckFalse = "回调返回失败";
    struct Binding {
        Kind kind = Kind::Boolean;
        void* address = nullptr;
        std::function<bool()> callback;
        std::vector<std::string> options;   // 仅 Kind::Select 使用，顺序与面板一致
        bool active = false;
        bool default_initialized = false;
        Json local_before;
    };
    struct RemoteValue {
        Json value, default_value;
        std::string source;
        TimePoint deadline = TimePoint::max();
    };
    struct Command {
        Json id;
        std::string id_key, key;
        TimePoint deadline;
        bool started = false;
    };
    struct Ack {
        Json id;
        std::string status, message;
    };
    struct Task {
        std::string id_key;
        TimePoint deadline;
        std::uint64_t epoch;
        std::function<bool()> callback;
    };
    struct ParsedResponse {
        std::string target;
        std::map<std::string, RemoteValue> values;
        std::vector<Command> commands;
        std::set<std::string> acked;
        int poll_after = 2;   // 服务端建议的轮询间隔（秒），1..30
    };

    RemoteConfig() {
        // 全局声明宏可能早于 main 构造单例；提前构造 runtime 的函数内静态依赖，
        // 使它们的析构晚于本单例 stop/join，避免退出时 worker 访问已析构 mutex。
        (void)JiYanSDK::GetRuntimeSnapshot();
    }
    ~RemoteConfig() {
        // 跨翻译单元静态析构次序不可控，此处绝不写绑定指针。
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
            changed_.notify_all();
        }
        if (worker_.joinable()) worker_.join();
    }

    static bool ValidKey(const std::string& key) {
        if (key.empty() || key.size() > 64) return false;
        const auto alpha = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
        if (!alpha(key[0])) return false;
        for (char c : key) if (!alpha(c) && !(c >= '0' && c <= '9') && c != '_') return false;
        return true;
    }
    static bool ValidTarget(const std::string& target) {
        if (target.size() != 64) return false;
        for (char c : target)
            if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F')) return false;
        return true;
    }
    static bool SameIdentity(const JiYanSDK::RuntimeSnapshot& a, const JiYanSDK::RuntimeSnapshot& b) {
        return a.generation == b.generation && a.verified == b.verified && a.identity_ready == b.identity_ready &&
               a.card_key == b.card_key &&
               a.device_id == b.device_id && a.player_uid == b.player_uid && a.api_url == b.api_url &&
               a.account.Account_Uid == b.account.Account_Uid && a.account.Account_Pass == b.account.Account_Pass &&
               a.account.Account_Token == b.account.Account_Token && a.app.App_Pid == b.app.App_Pid;
    }
    static bool ValidString(const Json& value) {
        if (!value.is_string()) return false;
        const std::string text = value.get<std::string>();
        if (text.empty() || text.size() > 64) return false;
        for (unsigned char ch : text)
            if (ch < 0x20 || ch == 0x7f) return false;
        return true;
    }

    static bool ValidValue(const Json& value, Kind kind) {
        if (kind == Kind::Boolean) return value.is_boolean();
        if (kind == Kind::String || kind == Kind::Select) return ValidString(value);
        if (kind == Kind::Button || !value.is_number()) return false;
        const double number = value.get<double>();
        if (!std::isfinite(number)) return false;
        if (kind == Kind::Integer)
            return std::trunc(number) == number && number >= std::numeric_limits<int>::min() &&
                   number <= std::numeric_limits<int>::max();
        return number >= -static_cast<double>(std::numeric_limits<float>::max()) &&
               number <= static_cast<double>(std::numeric_limits<float>::max());
    }
    static bool Scalar(const Json& value) {
        return value.is_boolean() || (value.is_number() && std::isfinite(value.get<double>())) ||
               value.is_string();
    }
    static Json ReadValue(const Binding& binding) {
        switch (binding.kind) {
        case Kind::Boolean: return *static_cast<bool*>(binding.address);
        case Kind::Integer: return *static_cast<int*>(binding.address);
        case Kind::Float: return *static_cast<float*>(binding.address);
        case Kind::String: return *static_cast<std::string*>(binding.address);
        // Select 本地保存的是索引；远程字符串在 WriteValue 里匹配成索引。
        case Kind::Select: return *static_cast<int*>(binding.address);
        default: return nullptr;
        }
    }
    static void WriteValue(Binding& binding, const Json& value) {
        switch (binding.kind) {
        case Kind::Boolean: *static_cast<bool*>(binding.address) = value.get<bool>(); break;
        case Kind::Integer: *static_cast<int*>(binding.address) = static_cast<int>(value.get<double>()); break;
        case Kind::Float: *static_cast<float*>(binding.address) = value.get<float>(); break;
        case Kind::String:
            if (value.is_string()) *static_cast<std::string*>(binding.address) = value.get<std::string>();
            break;
        case Kind::Select: {
            int* index = static_cast<int*>(binding.address);
            if (value.is_string()) {
                const std::string text = value.get<std::string>();
                for (std::size_t i = 0; i < binding.options.size(); ++i) {
                    if (binding.options[i] == text) {
                        *index = static_cast<int>(i);
                        return;
                    }
                }
                // 远程值不在本地下拉表内：保持本地选择，不视为失败。
                return;
            }
            if (value.is_number()) {
                const int wanted = static_cast<int>(value.get<double>());
                if (wanted >= 0 && static_cast<std::size_t>(wanted) < binding.options.size()) *index = wanted;
            }
            break;
        }
        default: break;
        }
    }
    static void RestoreLocal(Binding& binding) {
        if (binding.active) {
            WriteValue(binding, binding.local_before);
            binding.active = false;
        }
    }
    static void ReleaseDefault(Binding& binding, const Json& value) {
        WriteValue(binding, value);
        binding.active = false;
        binding.default_initialized = true;
    }
    bool RegisterValue(const std::string& key, void* value, Kind kind) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ValidKey(key)) return FailLocked("invalid feature key (use [A-Za-z][A-Za-z0-9_]{0,63})");
        if (!value) return FailLocked("binding address is null");
        const auto existing = bindings_.find(key);
        if (existing != bindings_.end()) {
            if (existing->second.address == value && existing->second.kind == kind) return true;
            return FailLocked("feature key is already bound: " + key);
        }
        for (const auto& item : bindings_)
            if (item.second.address == value) return FailLocked("address is already bound to: " + item.first);
        if (bindings_.size() >= MaxBindings) return FailLocked("at most 100 features may be bound");
        Binding binding;
        binding.kind = kind;
        binding.address = value;
        bindings_.emplace(key, std::move(binding)); // 登记不读取指针，不联网。
        changed_.notify_all();
        return true;
    }
    bool Fail(const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        return FailLocked(error);
    }
    bool FailLocked(const std::string& error) { last_error_ = error; return false; }
    void ClearRemoteLocked() {
        remote_values_.clear();
        pending_commands_.clear();
        acks_.clear();
        seen_commands_.clear();
        target_.clear();
        ++identity_epoch_;
    }
    void ObserveIdentityLocked(const JiYanSDK::RuntimeSnapshot& snapshot) {
        if (!have_identity_ || !SameIdentity(snapshot, identity_)) {
            identity_ = snapshot;
            have_identity_ = true;
            ClearRemoteLocked();
            restore_pending_ = true;
            changed_.notify_all();
        }
    }
    void PruneSeenLocked(TimePoint now) {
        for (auto it = seen_commands_.begin(); it != seen_commands_.end();) {
            // 未确认 ACK 始终保留去重记录；期限后重发的命令也已过期，不会再次回调。
            if (it->second <= now && !acks_.count(it->first)) it = seen_commands_.erase(it);
            else ++it;
        }
    }
    static std::string AckMessage(const std::string& message) {
        // 服务端限制 255 个字符；保守限制 240 bytes，且不切断 UTF-8 字符。
        // 用户异常的 what() 可能含无效字节，替换为 '?'，避免 JSON dump 拒整批 ACK。
        std::string safe;
        safe.reserve(std::min<std::size_t>(message.size(), 240));
        for (std::size_t i = 0; i < message.size() && safe.size() < 240;) {
            const auto first = static_cast<unsigned char>(message[i]);
            std::size_t width = 1;
            std::uint32_t codepoint = first;
            bool valid = first < 0x80;
            if (first >= 0xC2 && first <= 0xDF) { width = 2; codepoint = first & 0x1F; }
            else if (first >= 0xE0 && first <= 0xEF) { width = 3; codepoint = first & 0x0F; }
            else if (first >= 0xF0 && first <= 0xF4) { width = 4; codepoint = first & 0x07; }
            if (width > 1 && i + width <= message.size()) {
                valid = true;
                for (std::size_t k = 1; k < width; ++k) {
                    const auto next = static_cast<unsigned char>(message[i + k]);
                    if ((next & 0xC0) != 0x80) { valid = false; break; }
                    codepoint = (codepoint << 6) | (next & 0x3F);
                }
                if ((width == 2 && codepoint < 0x80) || (width == 3 && codepoint < 0x800) ||
                    (width == 4 && codepoint < 0x10000) || codepoint > 0x10FFFF ||
                    (codepoint >= 0xD800 && codepoint <= 0xDFFF)) valid = false;
            }
            if (!valid) { safe.push_back('?'); ++i; continue; }
            if (safe.size() + width > 240) break;
            safe.append(message, i, width);
            i += width;
        }
        return safe;
    }
    void SetAckLocked(const std::string& id, const std::string& status, const std::string& message) {
        const auto found = acks_.find(id);
        if (found != acks_.end()) { found->second.status = status; found->second.message = AckMessage(message); }
    }
    static double Timestamp(const Json& value, const char* field) {
        if (!value.is_number()) throw std::runtime_error(std::string("invalid ") + field);
        const double number = value.get<double>();
        if (!std::isfinite(number) || number < 0 || std::trunc(number) != number || number > 1.0e15)
            throw std::runtime_error(std::string("invalid ") + field);
        return number;
    }
    static TimePoint Deadline(double expiry, double server_time, TimePoint received) {
        if (expiry == 0) return TimePoint::max();
        if (expiry <= server_time) return received;
        const double seconds = expiry - server_time;
        const double capacity = std::chrono::duration<double>(TimePoint::max() - received).count();
        if (seconds >= capacity) return TimePoint::max();
        return received + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    }
    static std::string IdKey(const Json& id) {
        if (id.is_string()) {
            const auto value = id.get<std::string>();
            if (value.empty() || value.size() > 128) throw std::runtime_error("invalid command id");
            return "s:" + value;
        }
        if (id.is_number_unsigned()) {
            if (id.get<std::uint64_t>() == 0) throw std::runtime_error("invalid command id");
            return "n:" + id.dump();
        }
        if (id.is_number_integer() && id.get<std::int64_t>() > 0) return "n:" + id.dump();
        throw std::runtime_error("invalid command id");
    }
    static ParsedResponse ParseResponse(const std::string& text,
                                        const std::map<std::string, Kind>& kinds,
                                        TimePoint received) {
        const Json root = Json::parse(text);
        if (!root.is_object() || !root.at("success").is_boolean()) throw std::runtime_error("invalid response");
        if (!root.at("success").get<bool>()) {
            std::string message = root.value("message", std::string("cloud server rejected request"));
            if (message.size() > 512) message.resize(512);
            throw std::runtime_error(message);
        }
        const Json& data = root.at("data");
        if (!data.is_object()) throw std::runtime_error("invalid cloud data");
        const double server_time = Timestamp(data.at("server_time"), "server_time");
        ParsedResponse parsed;
        // 服务端建议的轮询间隔。缺省 2 秒；越界一律回落默认值，不给服务端放大负载的机会。
        {
            const auto poll = data.find("poll_after");
            if (poll != data.end() && poll->is_number()) {
                const double raw = poll->get<double>();
                if (std::isfinite(raw) && raw >= 1 && raw <= 30 && std::trunc(raw) == raw)
                    parsed.poll_after = static_cast<int>(raw);
            }
        }
        parsed.target = data.at("target").get<std::string>();
        if (!ValidTarget(parsed.target)) throw std::runtime_error("invalid cloud target");
        const Json& values = data.at("values");
        if (!values.is_object() && !(values.is_array() && values.empty()))
            throw std::runtime_error("invalid cloud values");
        if (values.size() > MaxBindings) throw std::runtime_error("too many cloud values");
        if (values.is_object()) for (auto it = values.begin(); it != values.end(); ++it) {
            if (!ValidKey(it.key()) || !it.value().is_object()) throw std::runtime_error("invalid value entry");
            const Json& value = it.value();
            RemoteValue entry;
            entry.value = value.at("value");
            entry.default_value = value.at("default_value");
            entry.source = value.at("source").get<std::string>();
            if (entry.source != "default" && entry.source != "global" && entry.source != "user")
                throw std::runtime_error("invalid value source");
            if (!Scalar(entry.value) || !Scalar(entry.default_value)) throw std::runtime_error("invalid scalar value");
            const auto kind = kinds.find(it.key());
            if (kind != kinds.end() && kind->second != Kind::Button &&
                (!ValidValue(entry.value, kind->second) || !ValidValue(entry.default_value, kind->second)))
                throw std::runtime_error("value type/range mismatch: " + it.key());
            entry.deadline = Deadline(Timestamp(value.at("expires_at"), "expires_at"), server_time, received);
            if (kind != kinds.end() && kind->second != Kind::Button)
                parsed.values.emplace(it.key(), std::move(entry));
        }
        const Json& commands = data.at("commands");
        if (!commands.is_array() || commands.size() > MaxAcks) throw std::runtime_error("invalid commands");
        std::set<std::string> ids;
        for (const auto& value : commands) {
            Command command;
            command.id = value.at("id");
            command.id_key = IdKey(command.id);
            command.key = value.at("feature_key").get<std::string>();
            if (!ValidKey(command.key)) throw std::runtime_error("invalid command feature key");
            const double expiry = Timestamp(value.at("expires_at"), "command expires_at");
            // 按钮不支持永久命令，0 也作已过期处理并产生 failed ACK。
            command.deadline = expiry == 0 ? received : Deadline(expiry, server_time, received);
            if (!ids.insert(command.id_key).second) throw std::runtime_error("duplicate command id");
            parsed.commands.push_back(std::move(command));
        }
        const Json& acked = data.at("acked");
        if (!acked.is_array() || acked.size() > MaxAcks) throw std::runtime_error("invalid acked list");
        for (const auto& id : acked) parsed.acked.insert(IdKey(id));
        return parsed;
    }

    void Worker() {
        auto next_poll = Clock::now();
        for (;;) {
            const auto snapshot = JiYanSDK::GetRuntimeSnapshot();
            Json payload;
            std::map<std::string, Kind> kinds;
            std::set<std::string> sent_acks;
            std::uint64_t request_epoch = 0;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (stop_) return;
                const bool changed_identity = !have_identity_ || !SameIdentity(snapshot, identity_);
                ObserveIdentityLocked(snapshot);
                if (changed_identity) next_poll = Clock::now();
                if (bindings_.empty() || (!snapshot.verified || !snapshot.identity_ready) || snapshot.card_key.empty() || snapshot.device_id.empty()) {
                    changed_.wait_for(lock, std::chrono::milliseconds(100));
                    continue;
                }
                if (force_sync_) {
                    // RequestSync()：面板刚改完或用户点“立即同步”，跳过本轮等待。
                    force_sync_ = false;
                    next_poll = Clock::now();
                }
                if (Clock::now() < next_poll) {
                    changed_.wait_until(lock, std::min(next_poll, Clock::now() + std::chrono::milliseconds(100)));
                    continue;
                }
                Json capabilities = Json::array();
                for (const auto& binding : bindings_) {
                    capabilities.push_back(binding.first);
                    kinds.emplace(binding.first, binding.second.kind);
                }
                Json acknowledgements = Json::array();
                for (const auto& item : acks_) {
                    if (item.second.message == kAckPending) continue;
                    acknowledgements.push_back({{"id", item.second.id}, {"status", item.second.status},
                                                {"message", item.second.message}});
                    sent_acks.insert(item.first);
                }
                payload = {{"action", "sdk_sync"}, {"developer_uid", snapshot.account.Account_Uid},
                           {"password", snapshot.account.Account_Pass}, {"token", snapshot.account.Account_Token},
                           {"app_id", snapshot.app.App_Pid}, {"card_key", snapshot.card_key},
                           {"device_id", snapshot.device_id}, {"capabilities", capabilities.dump()}, {"acks", acknowledgements.dump()}};
                request_epoch = identity_epoch_;
            }
            std::string response, error;
            bool ok = false;
            ParsedResponse parsed;
            const auto request_started = Clock::now();
            try {
                payload.erase("action");
                const auto authenticated = JiYanProtocol::Exchange(snapshot, "cloud-client.php", "sdk_sync",
                                                                  payload, "cloud_control", true);
                if (!authenticated.ok) throw std::runtime_error(authenticated.message);
                response = authenticated.plain;
                ok = true;
                // 使用请求起点折算 TTL，保守扣除传输/解析耗时，避免慢响应延长命令有效期。
                parsed = ParseResponse(response, kinds, request_started);
            } catch (const std::exception& failure) {
                ok = false;
                error = failure.what();
            } catch (...) {
                ok = false;
                error = "cloud response processing failed";
            }
            // 请求期间换卡/切UID/注销：旧响应连同 ACK 都不得进入新身份。
            const auto current = JiYanSDK::GetRuntimeSnapshot();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stop_) return;
                ObserveIdentityLocked(current);
                if (!SameIdentity(snapshot, current) || request_epoch != identity_epoch_) {
                    next_poll = Clock::now();
                    continue;
                }
                // 由服务端下发节奏决定，默认 2 秒；失败时退 5 秒，避免断网把重试打成风暴。
                next_poll = Clock::now() + std::chrono::seconds(ok ? parsed.poll_after : 5);
                if (!ok) {
                    last_error_ = error.size() <= 512 ? error : error.substr(0, 512);
                    continue; // 网络失败保留尚未到期的覆盖，TTL 由主线程处理。
                }
                if (target_ != parsed.target) {
                    ClearRemoteLocked();
                    restore_pending_ = true;
                    target_ = parsed.target;
                }
                // 只有本请求实际发送的 ACK 才能由这次响应确认，防回调期间误删。
                for (const auto& id : parsed.acked) if (sent_acks.count(id)) acks_.erase(id);
                PruneSeenLocked(Clock::now());
                remote_values_ = std::move(parsed.values);
                for (auto& command : parsed.commands) {
                    if (seen_commands_.count(command.id_key) || pending_commands_.count(command.id_key)) continue;
                    if (pending_commands_.size() >= MaxAcks) break;
                    pending_commands_.emplace(command.id_key, std::move(command));
                }
                poll_after_ = parsed.poll_after;
                last_error_.clear();
            }
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::thread worker_;
    std::map<std::string, Binding> bindings_;
    std::map<std::string, RemoteValue> remote_values_;
    std::map<std::string, Command> pending_commands_;
    std::map<std::string, Ack> acks_;
    std::map<std::string, TimePoint> seen_commands_;
    JiYanSDK::RuntimeSnapshot identity_;
    bool have_identity_ = false;
    bool stop_ = false;
    bool stopped_ = false;
    bool updating_ = false;
    bool restore_pending_ = false;
    bool force_sync_ = false;
    int poll_after_ = 2;
    std::uint64_t identity_epoch_ = 0;
    std::string target_, last_error_;
};

#ifndef JY_JOIN_IMPL
#define JY_JOIN_IMPL(a, b) a##b
#endif
#ifndef JY_JOIN
#define JY_JOIN(a, b) JY_JOIN_IMPL(a, b)
#endif
#ifndef 云控绑定
#define 云控绑定(var, key) JY_JOIN(_jy_cloud_registration_, __COUNTER__) = RemoteConfig::GetInstance().BindValue(key, &(var))
#endif

#endif // JIYAN_SDK_CLOUD_CONTROL_CPP
