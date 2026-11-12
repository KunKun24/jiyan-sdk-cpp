# Quick Start

This page covers the shortest path from zero to a working integration: account configuration, initialization, card key validation, heartbeat, and the entry points for cloud control and cloud update. Field names and behavior follow the sources under `include/jiyan/`; when the server disagrees with a local assumption, trust the integration test result.

## Requirements

| Item | Requirement |
| --- | --- |
| C++ standard | C++17 (`CMAKE_CXX_STANDARD 17` in CMake) |
| Compiler | g++ or clang++ on desktop; the Android NDK toolchain on Android |
| JNI headers | `SDK_Init.cpp` includes `<jni.h>` unconditionally, so desktop builds also need `JAVA_HOME/include` and `JAVA_HOME/include/linux` |
| Networking | The SDK ships a POSIX-socket HTTP/1.1 client. It does not use libcurl and does not support TLS |
| OS interfaces | The sources use `dladdr` and `/proc/self/maps`, targeting Android and Linux |
| Third-party libraries | None beyond the bundled `json.hpp`; no OpenSSL or libsodium |
| Build | CMake 3.16+, or compile `SDK_Main.cpp` as a single translation unit |

## Account and App Configuration

`Account_Info` describes the developer account and `App_Info` describes the application. Both are declared in `SDK_Init.cpp`.

| Field | Type | Meaning |
| --- | --- | --- |
| `Account_Uid` | `std::string` | Developer account UID; also the key for encryption method 3 |
| `Account_Pass` | `std::string` | Request password, sent when non-empty |
| `Account_Token` | `std::string` | Request token, sent when non-empty |
| `App_Pid` | `std::string` | Application ID; also part of the signature canonical string |
| `Crypto_Method` | `int` | Encryption method 0–8; must match the console |
| `Crypto_Key` | `std::string` | Encryption key; required by methods 1, 4, 5, 6, 7 and 8 |
| `Signature_Method` | `int` | Signature method 0–14; must match the console |
| `Signature_Key` | `std::string` | Signature key; required by every non-zero method |

`Signature_Method = 0` disables signing and `Signature_Key` may stay empty. Any other method with an empty key fails locally with `Signature_Key 未配置` before a request is sent. Encryption method 2 uses the first 64 characters of `Crypto_Key` as a custom Base64 alphabet (the key must be at least 64 bytes); method 3 uses `Account_Uid` instead of `Crypto_Key`.

## Initialization Order

1. Call `JiYanSDK::set_device_id(...)` with a stable, non-empty device identifier.
2. Fill in `Account_Info` and `App_Info`, then call `JiYanSDK::init(account, app)`.
3. Call `JiYanSDK::set_api_url(...)`. Only `http://` is supported and the path must end with `/api/`.
4. Optionally call `JiYanSDK::set_extra_params({...})` to add business fields.
5. Call `JiYanSDK_API::卡密验证::使用卡密(...)`. On success the SDK starts the heartbeat thread automatically.
6. For cloud control, register bindings first, call `JiYanSDK_API::云控::更新()` once per frame, and call `RemoteConfig::GetInstance().Shutdown()` before exit.

`set_api_url` strips trailing `/` and spaces, restores the default on an empty string, prepends `http://` when the scheme is missing, and appends `/api` only when the URL has no path at all. A custom path therefore has to include `/api` itself; otherwise requests land in the wrong directory.

## Card Key, Heartbeat and Cloud Control

| API | Purpose | When to call |
| --- | --- | --- |
| `卡密验证::使用卡密(key, ok, msg, cb)` | Validate and log in a card key; starts heartbeat on success | Login screen or button |
| `卡密验证::查询卡密(key, ok, msg, cb)` | Read expiry, seconds left and card type | Whenever card details are shown |
| `卡密验证::心跳(key, ok, msg, cb)` | Send one heartbeat manually | Rarely needed; the automatic thread is running |
| `卡密验证::解绑设备(key, ok, msg, cb)` | Unbind this device and stop heartbeat on success | User-initiated unbind |
| `卡密验证::获取公告()` | Fetch the announcement text synchronously | After entering the main screen |
| `同行识别::刷新信息(name, uid, ok, msg, cb)` | Report player identity for peer detection | When player name or UID changes |
| `云控::绑定开关/整数/小数/文本/下拉/按钮` | Register a remotely controlled value or button | Once during initialization |
| `云控::更新()` | Apply remote values and run pending button commands | Once per frame |
| `云更新::检查版本 / 获取下载地址` | Cloud update check and download URL retrieval | On a worker thread |

The heartbeat thread starts after a successful login and uses an interval derived from the session fields. Calling `使用卡密` again stops the old thread first, and a successful unbind also stops the heartbeat. Two consecutive heartbeat failures lock the SDK and clear the verified card key, so the user has to validate again.

## Minimal Example

The example enables synchronous mode for readability in a console program. For a UI program, remove `Enable_SingleThreaded` and read the async notes below.

```cpp
#include "jiyan/SDK_Main.cpp"
#include <iostream>
#include <string>

int main()
{
    JiYanSDK::Enable_SingleThreaded();          // console: run requests synchronously
    JiYanSDK::set_device_id("YOUR-DEVICE-ID");  // stable and non-empty

    Account_Info account;
    account.Account_Uid   = "YOUR-ACCOUNT-UID";
    account.Account_Pass  = "YOUR-ACCOUNT-PASS";
    account.Account_Token = "YOUR-ACCOUNT-TOKEN";

    App_Info app;
    app.App_Pid          = "YOUR-APP-ID";
    app.Crypto_Method    = 0;    // 0-8, must match the console
    app.Crypto_Key       = "";   // required by most non-zero methods
    app.Signature_Method = 0;    // 0-14, must match the console
    app.Signature_Key    = "";   // required when non-zero

    JiYanSDK::init(account, app);
    JiYanSDK::set_api_url("http://api.example.com/api/");
    JiYanSDK::set_extra_params({{"channel", "pc"}});

    bool ok = false;
    std::string msg;
    JiYanSDK_API::卡密验证::使用卡密("YOUR-CARD-KEY", ok, msg);
    if (!ok)
    {
        std::cerr << "validation failed: " << msg << "\n";
        return 1;
    }

    std::cout << "expires: " << JiYanSDK_Result::Data.card.expire_text << "\n";
    std::cout << "online: " << JiYanSDK_API::同行识别::在线人数() << "\n";
    std::cout << "announcement: " << JiYanSDK_API::卡密验证::获取公告() << "\n";

    ::Stop_Heartbeat();  // stop the background heartbeat thread before exit
    return 0;
}
```

## Async Mode and Callbacks

By default, `使用卡密`, `解绑设备`, `查询卡密`, `刷新信息` and similar APIs run the request on a detached thread and return immediately. The `bool&` and `std::string&` arguments are written asynchronously: if they are local variables, the references may dangle after the function returns. Keep them static, global or member-owned, or read them only from the completion callback. `心跳` and `获取公告` use the synchronous path and always complete on the calling thread.

Callbacks run on the request thread, not the UI thread. Do not touch ImGui, Android views or other thread-affine objects directly from a callback; copy the result into a queue and update the UI on the main thread. The global `Request_Completed_Callback` follows the same rule.

Button callbacks registered with `RemoteConfig` run on the thread that calls `Update()`, never on the network worker thread.

## Common Pitfalls

| Symptom | Cause | Fix |
| --- | --- | --- |
| The server cannot identify the device; download URL returns `设备码不能为空` | `set_device_id` was not called or the value is empty | Set a stable non-empty device ID at startup |
| HTTP 404 or `接口不存在` | The `api_url` path is not `/api`, or the trailing slash is missing | Use `http://.../api/`; custom paths must include `/api` |
| `纯 Socket HTTP 不支持 HTTPS` | The SDK has no TLS support | Use an `http://` URL; TLS termination is outside the SDK |
| `Signature_Key 未配置` | A non-zero signature method has an empty key | Provide the key, or set the method to 0 |
| `Crypto_Key 未配置` | Methods 1, 4, 5, 6, 7 or 8 have no key | Match the console configuration |
| Response decryption fails (1002) | Encryption method or key differs from the console | Verify both the ID and the key |
| Response signature check fails (1003) | Signature method, key or canonical rules differ | See [Signature](signature.md) |
| Async `ok`/`msg` contains garbage or crashes | The referenced object died before the request finished | Use static storage or synchronous mode |
| A second request does nothing | `is_requesting` is still set and the API returns early | Wait for the callback or serialize calls |
| Heartbeats are not as frequent as expected | The source waits with `std::chrono::minutes(interval)` | Trust the source and integration results, not only the seconds-level wording in the README |

## Next Steps

- [Encryption](crypto.md): methods 0–8, key handling and interoperability vectors.
- [Signature](signature.md): methods 0–14, the canonical string and method 14 key derivation.
- [Cloud Control and Cloud Update](cloud.md): switches, announcements, button ACKs, manifests and download receipts.
- [Errors](errors.md): codes, message sources and troubleshooting.
- [Compatibility](compat.md): 1.8 versus older versions, server matrix, threading and upgrade advice.
