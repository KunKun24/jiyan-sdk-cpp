# 安全策略 / Security Policy

## 支持版本

| 版本 | 支持 |
| --- | --- |
| 1.8.x | ✅ 安全更新 |
| 1.7.x | ✅ 关键修复 |
| ≤ 1.6 | ❌ 建议升级 |

## 报告漏洞

**请不要公开提交 Issue。** 通过以下任一渠道私下联系：

- Telegram：[@sad1881](https://t.me/sad1881)
- QQ：1106600707
- 官网控制台工单：<https://jiyan.xn--0try77c6um82w.cn/前台/开发者登录.php>

请附：SDK 版本、平台、复现步骤、影响范围；如可能，给出最小复现代码。
我们会在 72 小时内确认收到，并在修复后于 Release Notes 中致谢（可匿名）。

## 范围

本仓库只包含客户端 SDK。以下属于受理范围：

- 加密 / 签名实现的正确性缺陷（可导致密文可预测、验签绕过）
- 内存安全问题（越界、UAF、整数溢出）
- 随机数与密钥派生问题
- 协议解析缺陷（JSON / HTTP 响应处理）

以下不属于本仓库范围：

- 服务端接口、卡密系统、管理后台（请走官方工单）
- 注入器、内存修改、绕过检测类问题
- 社会工程、DDoS、暴力破解

## 安全建议

- 不要把 `Crypto_Key` / `Signature_Key` 硬编码在可被反编译的位置；至少做一层运行时派生。
- 使用签名 12 及以上；免费档也建议至少 12，避免 `hash(content + key)` 的长度扩展风险。
- 设备码必须稳定、非空，且不要在多台设备间共享。
- 发现密钥泄露立即在后台轮换，并重新打包客户端。

---

# Security Policy (English)

Please do **not** open a public issue for vulnerabilities. Contact us privately via
Telegram [@sad1881](https://t.me/sad1881) or the official console ticket.
We acknowledge within 72 hours and credit reporters in release notes.

Scope: client-side crypto/signature correctness, memory safety, RNG/key derivation,
protocol parsing. Out of scope: server, card system, admin panel, injectors.
