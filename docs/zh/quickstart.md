# 快速开始

本页把 JiYan Cloud C++ SDK 接入一个应用所需的最短路径串起来：准备账号信息、初始化、验证卡密、维持心跳，以及接入云控和云更新的入口。所有字段名与行为以 `include/jiyan/` 下的源码为准；与服务端不一致时，以实际联调结果为准。

## 环境要求

| 项目 | 要求 |
| --- | --- |
| C++ 标准 | C++17（CMake 中为 `CMAKE_CXX_STANDARD 17`） |
| 编译器 | 桌面端 g++ / clang++；Android 使用 NDK 工具链 |
| JNI 头文件 | `SDK_Init.cpp` 无条件 `#include <jni.h>`，桌面编译也要提供 `JAVA_HOME/include` 与 `JAVA_HOME/include/linux` |
| 网络 | SDK 自带 POSIX Socket HTTP/1.1 客户端，不依赖 libcurl，也不支持 TLS |
| 系统接口 | 源码使用 `dladdr`、`/proc/self/maps`，面向 Android / Linux |
| 第三方库 | 无 OpenSSL、libsodium 等依赖；自带 `json.hpp` |
| 构建 | CMake 3.16+，或把 `SDK_Main.cpp` 作为单一编译单元直接编译 |

## 账号与应用信息

`Account_Info` 对应开发者账号，`App_Info` 对应应用配置。两个结构体在 `SDK_Init.cpp` 中定义。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `Account_Uid` | `std::string` | 开发者账号 UID；加密方式 3 也用它作为密钥 |
| `Account_Pass` | `std::string` | 请求密码；非空时随请求发送 |
| `Account_Token` | `std::string` | 请求 Token；非空时随请求发送 |
| `App_Pid` | `std::string` | 应用 ID，同时参与签名 canonical 串 |
| `Crypto_Method` | `int` | 加密方式 0–8，必须与控制台配置一致 |
| `Crypto_Key` | `std::string` | 加密密钥；方式 1、4、5、6、7、8 必填 |
| `Signature_Method` | `int` | 签名方式 0–14，必须与控制台配置一致 |
| `Signature_Key` | `std::string` | 签名密钥；方式非 0 时必填 |

`Signature_Method = 0` 表示不签名；此时 `Signature_Key` 可以留空。其余方式留空会在发请求前直接报 `Signature_Key 未配置`。加密方式 2 把 `Crypto_Key` 的前 64 个字符当作自定义 Base64 字母表（密钥至少 64 字节），方式 3 不使用 `Crypto_Key` 而使用 `Account_Uid`。

## 初始化顺序

1. 调用 `JiYanSDK::set_device_id(...)`，传入稳定且非空的设备标识。
2. 填充 `Account_Info` 与 `App_Info`，调用 `JiYanSDK::init(account, app)`。
3. 调用 `JiYanSDK::set_api_url(...)`。只支持 `http://`，路径需以 `/api/` 结尾。
4. 可选：调用 `JiYanSDK::set_extra_params({...})` 追加业务字段。
5. 调用 `JiYanSDK_API::卡密验证::使用卡密(...)`；成功后 SDK 会自动启动心跳线程。
6. 需要云控时先登记绑定，再在每帧调用 `JiYanSDK_API::云控::更新()`；退出前调用 `RemoteConfig::GetInstance().Shutdown()`。

`set_api_url` 会去掉末尾的 `/` 和空格；空串恢复默认地址；缺少协议时补 `http://`；只有 URL 完全没有路径时才自动补 `/api`。因此自定义路径必须自己带上 `/api`，否则接口会落到错误目录。

## 卡密、心跳与云控

| 接口 | 作用 | 调用时机 |
| --- | --- | --- |
| `卡密验证::使用卡密(key, ok, msg, cb)` | 验证并登录卡密，成功后自动启动心跳 | 启动页或登录按钮 |
| `卡密验证::查询卡密(key, ok, msg, cb)` | 查询到期时间、剩余秒、卡类型 | 需要展示卡信息时 |
| `卡密验证::心跳(key, ok, msg, cb)` | 手动发一次心跳 | 一般不需要，自动线程已在跑 |
| `卡密验证::解绑设备(key, ok, msg, cb)` | 解绑当前设备，成功后停止心跳 | 用户主动解绑时 |
| `卡密验证::获取公告()` | 拉取公告文本，同步返回 | 进入主界面后 |
| `同行识别::刷新信息(name, uid, ok, msg, cb)` | 上报玩家身份，供同行识别 | 玩家名/UID 变化时 |
| `云控::绑定开关/整数/小数/文本/下拉/按钮` | 登记可远程控制的变量或按钮 | 初始化时一次性登记 |
| `云控::更新()` | 应用远程值并执行待处理按钮命令 | 每帧调用 |
| `云更新::检查版本 / 获取下载地址` | 云更新检查与取链接 | 业务工作线程 |

心跳线程在登录成功后启动，由 SDK 内部按会话字段控制间隔；再次调用 `使用卡密` 会先停止旧线程，`解绑设备` 成功后也会停止心跳。连续两次心跳失败会把 SDK 置为锁定状态并清空已验证卡密，需要重新验证。

## 最小示例

以下示例使用同步模式，便于控制台程序阅读。UI 程序请去掉 `Enable_SingleThreaded`，并阅读下一节的异步注意事项。

```cpp
#include "jiyan/SDK_Main.cpp"
#include <iostream>
#include <string>

int main()
{
    JiYanSDK::Enable_SingleThreaded();          // 控制台程序：请求同步执行
    JiYanSDK::set_device_id("YOUR-DEVICE-ID");  // 稳定、非空

    Account_Info account;
    account.Account_Uid   = "YOUR-ACCOUNT-UID";
    account.Account_Pass  = "YOUR-ACCOUNT-PASS";
    account.Account_Token = "YOUR-ACCOUNT-TOKEN";

    App_Info app;
    app.App_Pid          = "YOUR-APP-ID";
    app.Crypto_Method    = 0;    // 0-8，与后台一致
    app.Crypto_Key       = "";   // 非 0 的多数方式必填
    app.Signature_Method = 0;    // 0-14，与后台一致
    app.Signature_Key    = "";   // 非 0 必填

    JiYanSDK::init(account, app);
    JiYanSDK::set_api_url("http://api.example.com/api/");
    JiYanSDK::set_extra_params({{"channel", "pc"}});

    bool ok = false;
    std::string msg;
    JiYanSDK_API::卡密验证::使用卡密("YOUR-CARD-KEY", ok, msg);
    if (!ok)
    {
        std::cerr << "验证失败: " << msg << "\n";
        return 1;
    }

    std::cout << "到期时间: " << JiYanSDK_Result::Data.card.expire_text << "\n";
    std::cout << "在线人数: " << JiYanSDK_API::同行识别::在线人数() << "\n";
    std::cout << "公告: " << JiYanSDK_API::卡密验证::获取公告() << "\n";

    ::Stop_Heartbeat();  // 退出前停止后台心跳线程
    return 0;
}
```

## 异步模式与回调

默认情况下，`使用卡密`、`解绑设备`、`查询卡密`、`刷新信息` 等接口会把请求放到 detach 的线程里执行，并立即返回。传入的 `bool&`、`std::string&` 是异步写入的引用：如果它们是函数局部变量，函数返回后引用就可能悬空。请使用静态/成员变量，或等到回调触发后再读取。`心跳` 与 `获取公告` 走同步路径，始终在当前线程完成。

回调在请求线程执行，不在 UI 线程。不要在回调里直接操作 ImGui、Android View 等线程受限对象；把结果复制到队列，回到主线程再更新界面。全局的 `Request_Completed_Callback` 同样在请求线程触发。

`RemoteConfig::Update()` 的按钮回调则在调用 `Update()` 的线程执行；它不会在网络线程里执行。

## 常见坑

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 服务端无法区分设备，云更新取链接报 `设备码不能为空` | `set_device_id` 为空或未调用 | 启动时设置稳定非空设备码 |
| 返回 404 或 `接口不存在` | `api_url` 路径不是 `/api`，或末尾缺 `/` | 使用 `http://.../api/`；自定义路径要自带 `/api` |
| `纯 Socket HTTP 不支持 HTTPS` | SDK 不支持 TLS | 改用 `http://` 地址；TLS 终止不在 SDK 内 |
| `Signature_Key 未配置` | 签名方式非 0 但密钥为空 | 填密钥，或把方式改为 0 |
| `Crypto_Key 未配置` | 方式 1、4、5、6、7、8 缺少密钥 | 与控制台核对密钥 |
| 响应解密失败（1002） | 加密方式或密钥与控制台不一致 | 逐项核对编号与密钥 |
| 响应签名校验失败（1003） | 签名方式、密钥或 canonical 口径不一致 | 见 [签名方式](signature.md) |
| 异步接口的 `ok`/`msg` 读到乱值或崩溃 | 引用在请求完成前已失效 | 改静态变量或使用同步模式 |
| 第二次请求没有任何反应 | `is_requesting` 仍为真，接口直接返回 | 等上一次回调结束，或串行化调用 |
| 心跳没有按预期频繁发送 | 源码以 `std::chrono::minutes(interval)` 等待 | 以源码和联调结果为准，不要只按 README 的秒级描述假设 |

## 下一步

- [加密方式](crypto.md)：0–8 的算法、密钥口径与互操作测试向量。
- [签名方式](signature.md)：0–14、canonical 串与 14 的密钥派生。
- [云控与云更新](cloud.md)：开关、公告、按钮 ACK、清单与下载回执。
- [错误处理](errors.md)：错误码、文案来源与排查建议。
- [兼容性](compat.md)：1.8 与旧版、服务端矩阵、线程模型与升级建议。
