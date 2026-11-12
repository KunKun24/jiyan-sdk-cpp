# 更新日志 / Changelog

所有版本遵循 [SemVer](https://semver.org/lang/zh-CN/)。

## 1.8.0 — 2026-11-12

### 新增

- **加密 6**：AES-256-GCM，nonce 12 字节，输出 `base64(nonce‖ct‖tag16)`
- **加密 7**：ChaCha20-Poly1305（RFC 8439），nonce 12 字节
- **加密 8**：XChaCha20-Poly1305，nonce 24 字节（HChaCha20 派生子密钥）
- **签名 14**：BLAKE2b-256 带密钥，`key = SHA256(sign_key)` 原始 32 字节，输出 64 位小写 hex
- 新增 `SDK_TLS/Crypto/AES256GCM.cpp`、`SDK_TLS/Crypto/ChaCha20Poly1305.cpp`、
  `SDK_TLS/Signature/BLAKE2b.cpp`（均为 header-only，复用现有 AES / SHA-256）
- 新增 `tests/crypto_vectors.cpp`：NIST GCM、RFC 8439、RFC 7693 标准向量 + 往返 / 篡改测试

### 调整

- `JiYan_Crypto.cpp`：switch 扩到 8，新增 `JiYan_Key32()` 统一密钥口径
  （Base64 严格解码成功且 ≥16 字节用解码值，否则原文；补零 / 截断 32 字节）
- `JiYan_Signature.cpp`：if 链扩到 14
- `SDK_Protocol.cpp`：算法上界改为 8 / 14；6/7/8 必须配置 `Crypto_Key`
- `SDK_Main.cpp`：版本号 `1.8`
- 中文接口示例注释更新为 0–8 / 0–14

### 兼容性

- 接口、参数、返回结构不变；旧客户端无需改动
- 加密 5–8、签名 13–14 为服务端会员专享，其余免费

## 1.7.0 — 2026-09

- 中文 API、开放建卡、同行识别、在线人数、当前库状态、云控示例
- 错误前缀统一为 `JiYan_Error`
- 心跳改为 10 秒，配合服务端在线窗口

## 1.6.0 及更早

- 多应用、云更新清单校验、注入云更新、设备码与 JNI 适配等
- 历史细节见官方更新内容文件与 Release Notes
