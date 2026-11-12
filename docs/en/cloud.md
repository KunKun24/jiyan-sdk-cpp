# Cloud Control and Cloud Update

Cloud control and cloud update are separate features. Cloud control pulls remote switches, announcements and button commands from `cloud-client.php`; cloud update checks published files, obtains a download URL and submits a receipt through `Cloudupdate.php`. Both use the same account and app configuration. Cloud control requires a validated card session and a non-empty device ID; cloud update also requires a non-empty device ID when obtaining a download URL. The server internals are not part of the open-source scope; this page describes client behavior only, and field semantics follow the source.

## Cloud Control: Bindings

| Panel type | Chinese API | Variable type | Behavior |
| --- | --- | --- | --- |
| Switch | `绑定开关(key, var)` | `bool*` | Toggled remotely |
| Numeric slider | `绑定整数` / `绑定小数` | `int*` / `float*` | Changed remotely |
| Text | `绑定文本(key, var)` | `std::string*` | Text pushed remotely |
| Select | `绑定下拉(key, index, options)` | `int*` + option list | Index maps to option order |
| One-shot command | `绑定按钮(key, callback)` | `std::function<void()>` | Runs once per command |

Keys must match `^[A-Za-z][A-Za-z0-9_]{0,63}$`, with at most 100 bindings. Select lists accept 1–100 options of 1–64 bytes each, with no duplicates. Duplicate keys or duplicate variable addresses are rejected, and the reason is stored in `最后错误()`. Binding itself does not read the pointer or use the network, and the variable must outlive the binding.

```cpp
#include "jiyan/SDK_Main.cpp"

struct Config {
    bool show_fps = false;
    float menu_scale = 1.0f;
    int item_count = 10;
    int mode_index = 0;
    std::string hint = "welcome";
};

static Config g_config;
static const std::vector<std::string> kModes = {"stable", "performance", "extreme"};

void BindCloudControl() {
    JiYanSDK_API::云控::绑定开关("显示帧率", &g_config.show_fps);
    JiYanSDK_API::云控::绑定小数("菜单缩放", &g_config.menu_scale);
    JiYanSDK_API::云控::绑定整数("显示数量", &g_config.item_count);
    JiYanSDK_API::云控::绑定文本("提示文本", &g_config.hint);
    JiYanSDK_API::云控::绑定下拉("模式", &g_config.mode_index, kModes);
    JiYanSDK_API::云控::绑定按钮("重置布局", [] { /* business action */ });
}
```

## Cloud Control: Update Loop

Call `JiYanSDK_API::云控::更新()` once per frame from the render or main loop. Once the card is verified, the identity is ready and at least one binding exists, the first call starts a background worker. While the card is not verified, the identity is not ready, or `Shutdown()` has run, the call returns without any network traffic. The worker fetches and parses; button callbacks run on the thread that calls `更新()`.

```cpp
void OnFrame() {
    JiYanSDK_API::云控::更新();
}

void OnUserPressedSyncNow() {
    JiYanSDK_API::云控::立即同步();  // Skip the next polling wait
}
```

Remote values have three sources. `default` is written once and the local value is restored on release; `global` and `user` save the local value on first override, force-write on every `更新()`, and restore it on release or identity change. `expires_at` 0 means permanent for values (but already expired for buttons); an expired value falls back to `default_value`. The polling interval comes from the server's `poll_after`, clamped to 1–30 seconds and defaulting to 2. After a failure the client retries in about 5 seconds and keeps overrides until their TTL.

## Cloud Control: Buttons and ACKs

Every button command registers a pending ACK before the callback runs. An exception or a failed return affects only that ACK and does not stop the loop. ACK status is success or failure, with messages such as `执行成功`, `回调返回失败`, `回调异常：...`, `指令已过期，未执行` and `客户端未绑定该按钮`. Only ACKs actually sent in a request can be confirmed by the server; if the identity changes between the callback and the ACK, the stale ACK is dropped.

## Cloud Control: Status and Shutdown

`轮询间隔秒()` returns the server-suggested interval; an empty `最后错误()` means the last synchronization succeeded; `是否被远程覆盖(key)` reports whether an active `global`/`user` override exists. Call `RemoteConfig::GetInstance().Shutdown()` before exit to restore overrides and stop and join the worker. There is no Chinese wrapper for `Shutdown()`, and later `更新()` calls do not restart networking.

```cpp
// Shutdown sequence
JiYanSDK_API::云控::更新();  // Apply the last state before stopping
RemoteConfig::GetInstance().Shutdown();
```

## Cloud Update: Version Check

| API | Mode | Description |
| --- | --- | --- |
| `检查版本(so name, ok, msg)` | Async | Chinese compatibility API, checks by SO name and local MD5 |
| `检查版本按版本号(version, ok, msg)` | Async | Chinese compatibility API, checks by version |
| `CloudUpdate::CheckVersionSync(so name)` | Sync | Hashes the loaded SO, then checks |
| `CloudUpdate::CheckVersionByMD5(name, md5)` | Sync | Explicit local MD5, must be 32 hex characters |
| `CloudUpdate::CheckVersionByVersionSync(version)` | Sync | Server-side version comparison |

The result is `Cloud_Update_Result` with `request_ok`, `message` and `cloud`. `cloud` carries `need_update`, `has_file`, `file_id`, `file_name`, `file_size`, `latest_md5`, `version`, `download_url`, `ticket`, `source` and `update_content`. `source` is `local` or `remote`; `local` requires both `ticket` and `md5`. The client validates the fields the server returns and does not infer file locations.

```cpp
Cloud_Download_Result PrepareDownload(const std::string& so_name) {
    const auto check = JiYanSDK_API::CloudUpdate::CheckVersionSync(so_name);
    if (!check.request_ok || !check.cloud.has_file || check.cloud.need_update != 1)
        return {false, check.message, check.cloud, {}};
    return JiYanSDK_API::CloudUpdate::GetDownloadUrlById(
        check.cloud.file_id, check.cloud.file_name, check.cloud.latest_md5);
}
```

Do not send an empty digest when the local MD5 cannot be read; `CheckVersionSync` reports `本地 SO 文件无法读取或未加载，不能进行 MD5 检查`. A name mismatch also fails the check, which prevents downloading a different file.

## Cloud Update: Download and Receipt

The SDK does not implement a downloader. The host downloads from `download_url` with its own network stack, closes the file, and passes the snapshot stored by value from `Cloud_Download_Result::download` to `ConfirmDownload`. Before confirming, the SDK re-checks device, app, identity and generation, and recomputes the MD5 of the written file; any mismatch means no receipt is sent.

```cpp
Cloud_Update_Result ConfirmAfterWrite(const Cloud_Download_Snapshot& snapshot,
                                      const std::string& local_path) {
    const auto result = JiYanSDK_API::CloudUpdate::ConfirmDownload(snapshot, local_path);
    if (!result.request_ok) return result;  // Keep the snapshot and file, retry the same ticket
    // result.cloud.confirmed == 1 means the receipt succeeded
    // charged == 1 means this receipt charged a download; charged == 0 may be a duplicate or external link
    // remaining_times == -1 means the response did not include the remaining count
    return result;
}
```

If a receipt fails, do not immediately request a new ticket. When the network outcome is uncertain, keep the original snapshot and the complete file and retry the same ticket; the server does not charge twice for a repeated receipt. `GetFileList` is a management operation, and client calls return `新 HTTP 客户端包不开放管理操作，请在 HTTPS 主站后台操作`.

## Announcement and Library Status

`JiYanSDK_API::卡密验证::获取公告()` synchronously requests `get_announcement` from `key.php` and returns `data.announcement`, falling back to the response `message`. `云更新::当前库状态()` reads `/proc/self/maps`: 0 means the library was loaded from the `/JiYan_Cloud/` cloud-update directory, 1 means another custom path, and 2 means it was not detected.

Cloud control and cloud update both go through the same protocol layer, so encryption, signature, `rc` checking and error codes are identical. When troubleshooting, read `最后错误()` and `message` first, then consult [Errors](errors.md).
