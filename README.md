# 极验云 C++ SDK

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/KunKun24/jiyan-sdk-cpp?display_name=tag)](https://github.com/KunKun24/jiyan-sdk-cpp/releases)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Platform](https://img.shields.io/badge/platform-Android%20%7C%20Linux-lightgrey)

**中文** · [English](README.en.md)

极验云（JiYan Cloud）C++ 客户端 SDK：卡密验证、心跳防重放、云控、云更新、同行识别。
纯头文件 + 单编译单元，不依赖 OpenSSL / libsodium / libcurl，Android 与桌面端都能编。

> 服务端与后台不开源。本仓库只包含客户端接入层，官方云服务地址见下方「官方渠道」。

---

## 开源范围

| 内容 | 状态 |
| --- | --- |
| C++ 客户端 SDK（本仓库） | ✅ 开源（MIT） |
| 加密 / 签名算法实现与文档 | ✅ 开源 |
| 接入示例、错误码、协议说明 | ✅ 开源 |
| 服务端（验证接口、卡密、云控、云更新分发） | ❌ 闭源 |
| 管理后台、数据库结构、部署脚本 | ❌ 闭源 |
| 注入器 / 内存操作 / 绕过检测类工具 | ❌ 不提供 |

---

## 特性

- **加密 0–8**：不加密、AES-256-CBC、自定义 Base64、EnvProto、JiYan-Seal、AES-CBC+HMAC、AES-256-GCM、ChaCha20-Poly1305、XChaCha20-Poly1305
- **签名 0–14**：MD5 / SHA 系列、HMAC-SHA256、双段 HMAC、带密钥 BLAKE2b
- **心跳防重放**：独立线程，10 秒级心跳，断线不阻塞渲染
- **云控**：远程开关、公告、按钮回传、ACK
- **云更新**：版本比较、文件清单、MD5 校验、下载回执
- **同行识别 / 在线人数 / 当前库状态**
- **中文 API**：`JiYanSDK_API::卡密验证::使用卡密` 等，原生命令式接口
- **无第三方依赖**：json.hpp 已内置（MIT），TLS 走自有实现

## 快速开始

要求：C++17、Android NDK（或桌面 g++）、JDK 头文件（JNI）。

```cpp
#include "jiyan/SDK_Main.cpp"

Account_Info account;
account.Account_Uid   = "JiYan_0000001";   // 开发者 UID
account.Account_Pass  = "安全请求密码";
account.Account_Token = "安全请求Token";

App_Info app;
app.App_Pid          = "10001";            // 应用实例 UID
app.Crypto_Method    = 6;                  // 0-8
app.Crypto_Key       = "后台配置的加解密密钥";
app.Signature_Method = 14;                 // 0-14
app.Signature_Key    = "后台配置的签名密钥";

JiYanSDK::set_device_id(Get_AndroidID());  // 稳定且非空
JiYanSDK::init(account, app);
JiYanSDK::set_api_url("http://api.xn--0try77c6um82w.cn/api/");
```

卡密验证：

```cpp
bool ok = JiYanSDK_API::卡密验证::使用卡密("你的卡密");
auto info = JiYanSDK_Result::current_response_result;
```

完整可运行片段见 [`examples/`](examples/)。

## 加密与签名

| 编号 | 加密 | nonce / IV | 密文格式 |
| --- | --- | --- | --- |
| 0 | 不加密 | — | 原文 |
| 1 | AES-256-CBC + PKCS7 | 16 字节，响应回传 | `base64(ct)`，IV 单独返回 |
| 2 | 自定义 Base64 | — | 自定义字符集 |
| 3 | EnvProto | — | 按 UID 派生 |
| 4 | JiYan-Seal | — | 单串 Base64 |
| 5 | AES-CBC + HMAC | 16 字节 | 单串 Base64 |
| 6 | AES-256-GCM | 12 字节 | `base64(nonce‖ct‖tag16)` |
| 7 | ChaCha20-Poly1305 | 12 字节 | 同上 |
| 8 | XChaCha20-Poly1305 | 24 字节 | 同上 |

| 编号 | 签名 |
| --- | --- |
| 0–11 | MD5 / SHA1 / SHA224 / SHA256 / SHA384 / SHA512 / SHA3 / RIPEMD160 / CRC32 等 |
| 12 | HMAC-SHA256 |
| 13 | 双段 HMAC（`hmac + "." + hmac(prefix+content+key)[:16]`） |
| 14 | BLAKE2b-256，`key = SHA256(sign_key)` 原始 32 字节，输出 64 位小写 hex |

密钥口径：先按 Base64 严格解码，成功且 ≥16 字节则用解码结果，否则用原字符串；补 `\0` 或截断到 32 字节。
加密 5–8、签名 13–14 为服务端会员专享，其余免费；非会员调用会收到服务端提示。

## 目录结构

```
include/jiyan/        SDK 源码（SDK_Main.cpp 为唯一编译入口）
  SDK_TLS/            加密与签名实现
  json.hpp            nlohmann/json（MIT）
examples/             接入片段（部分依赖游戏/ImGui 环境）
docs/zh, docs/en      接入文档
tests/                算法自测（无需联网）
```

## 构建与测试

```bash
# 算法自测
g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o crypto_vectors
./crypto_vectors

# 编译 SDK（需要 JNI 头文件）
g++ -std=c++17 -O2 -c include/jiyan/SDK_Main.cpp \
    -Iinclude/jiyan -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux"

# 或 CMake
cmake -B build && cmake --build build
```

## 兼容性

| SDK | 服务端 | 说明 |
| --- | --- | --- |
| 1.8 | 2026-11 之后 | 新增加密 6/7/8、签名 14 |
| 1.7 | 2026-09 之后 | 基础算法 0–5 / 0–13 |
| 1.2–1.6 | 兼容 | 接口不变，建议升级 |

服务端升级不影响 1.7 及更早客户端；新算法需要 1.8+。

## 文档

- [快速开始](docs/zh/quickstart.md) · [Quick start](docs/en/quickstart.md)
- [加密规范](docs/zh/crypto.md) · [Encryption](docs/en/crypto.md)
- [签名规范](docs/zh/signature.md) · [Signature](docs/en/signature.md)
- [云控与云更新](docs/zh/cloud.md) · [Cloud control & update](docs/en/cloud.md)
- [错误码](docs/zh/errors.md) · [Error codes](docs/en/errors.md)
- [兼容性](docs/zh/compat.md) · [Compatibility](docs/en/compat.md)

## 官方渠道

- 官网：<https://jiyan.xn--0try77c6um82w.cn>
- 接口文档：<https://jiyan.xn--0try77c6um82w.cn/docs.html>（[English](https://jiyan.xn--0try77c6um82w.cn/en/docs.html)）
- 控制台：<https://jiyan.xn--0try77c6um82w.cn/前台/开发者登录.php>
- 社区：<http://shequ.xn--0try77c6um82w.cn>
- 开发者主页：<http://www.xn--0try77c6um82w.cn>
- Telegram：[@sad1881](https://t.me/sad1881) · 频道 [@JiYanWangLuo](https://t.me/JiYanWangLuo)
- QQ 群：1106600707

> 极验云与 Geetest（极验）无关。请只从本仓库或官方渠道获取 SDK。

## 安全

漏洞请勿公开提 issue，见 [SECURITY.md](SECURITY.md)。

## 许可证与版权

[MIT](LICENSE) © 2026 JiYan Cloud（极验云）· 和泉纱雾
内置 `json.hpp` 版权归 Niels Lohmann（MIT），见 [NOTICE.md](NOTICE.md)。

## 关于 AIGC

> 项目包含 AIGC 内容，AI 减少了开发时间；我们也认真审核了每一条代码。
