# JiYan Cloud C++ SDK

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/KunKun24/jiyan-sdk-cpp?display_name=tag)](https://github.com/KunKun24/jiyan-sdk-cpp/releases)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)

[中文](README.md) · **English**

Card-key validation, heartbeat, cloud control, cloud update and peer detection for
JiYan Cloud — in one header-only SDK with a single unity source file.
No OpenSSL, libsodium or libcurl required.

> The server and admin panel are closed source. This repository contains the client layer only.

---

## What is open

| Component | Status |
| --- | --- |
| C++ client SDK (this repo) | ✅ MIT |
| Crypto / signature implementations & docs | ✅ |
| Examples, error codes, protocol notes | ✅ |
| Server (validation, card keys, cloud control, update delivery) | ❌ |
| Admin panel, database schema, deployment | ❌ |
| Injectors / memory tools / detection bypass | ❌ |

## Features

- **Encryption 0–8**: none, AES-256-CBC, custom Base64, EnvProto, JiYan-Seal,
  AES-CBC+HMAC, AES-256-GCM, ChaCha20-Poly1305, XChaCha20-Poly1305
- **Signature 0–14**: MD5/SHA family, HMAC-SHA256, double HMAC, keyed BLAKE2b
- **Heartbeat**: dedicated thread, interval configured by the server (minutes), replay protection
- **Cloud control / cloud update / peer detection**
- **Chinese API** (`JiYanSDK_API::卡密验证::使用卡密`) and classic C++ API
- **No third-party runtime deps** — bundled nlohmann/json (MIT)

## Quick start

Requires C++17, NDK or g++, and JNI headers.

```cpp
#include "jiyan/SDK_Main.cpp"

Account_Info account;
account.Account_Uid   = "JiYan_0000001";
account.Account_Pass  = "request-password";
account.Account_Token = "request-token";

App_Info app;
app.App_Pid          = "10001";
app.Crypto_Method    = 6;                 // 0-8
app.Crypto_Key       = "your-crypto-key";
app.Signature_Method = 14;                // 0-14
app.Signature_Key    = "your-sign-key";

JiYanSDK::set_device_id(Get_AndroidID());
JiYanSDK::init(account, app);
JiYanSDK::set_api_url("http://api.xn--0try77c6um82w.cn/api/");
```

```cpp
bool ok = false;
std::string msg;
JiYanSDK_API::卡密验证::使用卡密("CARD-KEY", ok, msg, nullptr);
```

See [`examples/`](examples/) for full snippets.

## Crypto & signature

| ID | Encryption | Nonce | Output |
| --- | --- | --- | --- |
| 0 | none | — | plaintext |
| 1 | AES-256-CBC + PKCS7 | 16 B (returned) | `base64(ct)` |
| 2 | custom Base64 | — | custom alphabet |
| 3 | EnvProto | — | derived per UID |
| 4 | JiYan-Seal | — | single Base64 string |
| 5 | AES-CBC + HMAC | 16 B | single Base64 string |
| 6 | AES-256-GCM | 12 B | `base64(nonce‖ct‖tag16)` |
| 7 | ChaCha20-Poly1305 | 12 B | same |
| 8 | XChaCha20-Poly1305 | 24 B | same |

Signature IDs 12/13/14 are HMAC-SHA256, double HMAC and keyed BLAKE2b-256
(`key = SHA256(sign_key)`, 64 lowercase hex chars).
Key rule (encryption 6/7/8): strict Base64 first; if it decodes to ≥16 bytes use it,
else use the raw string padded/truncated to 32 bytes. Encryption 1 uses lenient Base64;
encryption 3 derives the key from `Account_Uid`; signature 14 uses `SHA256(sign_key)`.

Encryption 5–8 and signatures 13–14 require a server-side membership plan; the client
does not enforce it locally.

## Build & test

```bash
g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o crypto_vectors
./crypto_vectors

g++ -std=c++17 -O2 -c include/jiyan/SDK_Main.cpp \
    -Iinclude/jiyan -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux"
```

## Docs

[Quick start](docs/en/quickstart.md) · [Crypto](docs/en/crypto.md) ·
[Signature](docs/en/signature.md) · [Cloud](docs/en/cloud.md) ·
[Errors](docs/en/errors.md) · [Compatibility](docs/en/compat.md)

## Official channels

- Website: <https://jiyan.xn--0try77c6um82w.cn>
- Docs: <https://jiyan.xn--0try77c6um82w.cn/en/docs.html>
- Community: <http://shequ.xn--0try77c6um82w.cn>
- Telegram: [@sad1881](https://t.me/sad1881) · channel [@JiYanWangLuo](https://t.me/JiYanWangLuo)
- QQ group: 1106600707

> JiYan Cloud is not affiliated with Geetest.

## License

[MIT](LICENSE) © 2026 JiYan Cloud · 和泉纱雾.
Bundled `json.hpp` © Niels Lohmann, MIT — see [NOTICE.md](NOTICE.md).

> This project contains AIGC content; AI reduced development time and every change is reviewed.
