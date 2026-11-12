# 加密方式

加密发生在协议层：SDK 把请求参数拼成查询串，按 `Crypto_Method` 加密后放入外层 JSON 的 `data` 字段。响应体是加密后的字符串，SDK 解密后再验签、校验 `rc`。本页描述 0–8 的实现口径，全部以 `SDK_TLS/JiYan_Crypto.cpp` 及各算法文件为准。

## 总览

| ID | 名称 | 密钥来源 | IV / nonce | 输出格式 |
| --- | --- | --- | --- | --- |
| 0 | 不加密 | 无 | 无 | 明文查询串，`IV` 为空 |
| 1 | AES-256-CBC + PKCS7 | `Crypto_Key` 先 Base64 解码 | 16 字节随机，Base64 | `{"密文": base64(ct), "IV": base64(iv)}` |
| 2 | 自定义 Base64 | `Crypto_Key` 即 64 字符字母表 | 无 | 自定义字母表编码，带 `=` 填充 |
| 3 | EnvProto | `Account_Uid` | 无 | 小写 hex，每字节两字符 |
| 4 | JiYan-Seal | `Crypto_Key` 原始字节 | 16 字节随机 | base64url(nonce‖ct‖tag8)，无填充 |
| 5 | AES-256-CBC + HMAC | `Crypto_Key` 原始字节 | 16 字节随机 | base64url(iv‖ct‖tag16)，无填充 |
| 6 | AES-256-GCM | `Crypto_Key`（key32 口径） | 12 字节随机 | base64(nonce‖ct‖tag16) |
| 7 | ChaCha20-Poly1305 | `Crypto_Key`（key32 口径） | 12 字节随机 | base64(nonce‖ct‖tag16) |
| 8 | XChaCha20-Poly1305 | `Crypto_Key`（key32 口径） | 24 字节随机 | base64(nonce‖ct‖tag16) |

## 密钥口径

方式 6、7、8 使用统一的 `JiYan_Key32`：

1. 先做严格 Base64 解码。允许空白字符和省略的 `=`，拒绝非法字符、错误填充和长度模 4 余 1。
2. 解码成功且结果不少于 16 字节时，使用解码后的字节；否则使用 `Crypto_Key` 原文的字节。
3. 把结果补零或截断到恰好 32 字节。

也就是说，密钥是“先解码、再补零/截断”，不是先补齐再解码。方式 1 不同：它使用宽松 Base64 解码（未知字符按 0 处理），再把结果补零/截断到 32 字节。方式 4、5 直接把 `Crypto_Key` 原文作为 HMAC-SHA256 的密钥，不做 Base64 解码。方式 2 把 `Crypto_Key` 当作输出字母表，长度至少 64 字节，越界会读到未定义内容。方式 3 使用 `Account_Uid`，`Crypto_Key` 不参与。

## 报文格式

请求外层 JSON 固定为：

```cpp
{"developer_username": "...", "app_id": "...", "iv": "...", "data": "..."}
```

`data` 是加密后的查询串。响应体不是 JSON 外层，而是加密字符串本身：SDK 先检查是否以 `JiYan_Error` 开头，再按同一方式解密。方式 1 的解密使用本次请求发出的 IV（源码把 `request.iv` 传回解密函数），因此服务端必须用请求里的 IV 加密响应，不能假设响应带新 IV。方式 6、7、8 的 nonce 位于 `data` 头部，外层 `iv` 只是同一 nonce 的 Base64 副本。

## 各方式细节

方式 0：查询串原样发送，响应体是明文 JSON。`IV` 为空字符串。

方式 1：AES-256-CBC，PKCS7 始终填充。密文用标准 Base64 编码，与 IV 一起返回。密钥或 IV 无效时加密结果为空，协议层随后抛出 `本地加密失败，请检查 Crypto_Key 与 Crypto_Method`。

方式 2：把 3 字节一组映射为 4 个字符，字符取自 `Crypto_Key` 的对应下标，末尾按 Base64 规则补 `=`。解码时去掉尾部 `=`，逐字符在 `Crypto_Key` 中查找下标，找不到即失败。该方式只用于兼容，不提供密码学强度。

方式 3：设 `h = sum(byte(key[i]) * (i + 1)) % 100`，对第 i 个明文字节计算 `c = plain[i] XOR key[(i + h) % len(key)]`，再输出 `(c + h + i) % 256` 的小写 hex。解密按逆序还原。UID 为空会除零；非 ASCII 字符在加密与解密两端的符号性不同，建议只使用 ASCII UID。

方式 4：nonce 为 16 字节随机数。密钥流由若干 32 字节块拼接，每块为 `HMAC-SHA256(nonce ‖ be32(blockIndex), key)`。密文为明文异或密钥流，tag 为 `HMAC-SHA256(nonce ‖ ct, key)` 的前 8 字节。整体 base64url，无 `=` 填充；解密至少需要 24 字节原始数据。

方式 5：`encKey = HMAC-SHA256("jy1-enc", key)`，`macKey = HMAC-SHA256("jy1-mac", key)`。IV 为 16 字节随机数，密文为 AES-256-CBC-PKCS7，tag 为 `HMAC-SHA256(iv ‖ ct, macKey)` 前 16 字节。输出 base64url(iv‖ct‖tag16)，解密至少需要 32 字节原始数据，先验 tag 再解密。

方式 6：AES-256-CTR，计数器初值 `J0 = nonce ‖ 00 00 00 01`。无 AAD，GHASH 只覆盖密文，长度块为 `len(A)=0 ‖ len(C)`（64 位大端）。tag 为 `GHASH ^ AES_K(J0)` 的 16 字节，输出 ct‖tag。

方式 7：IETF ChaCha20-Poly1305，12 字节 nonce。Poly1305 一次性密钥来自计数器 0 的 ChaCha20 块，数据从计数器 1 开始，tag 16 字节，输出 ct‖tag。

方式 8：24 字节 nonce。先用 HChaCha20 以 key 和前 16 字节 nonce 派生子密钥，再以 `00 00 00 00 ‖ nonce[16..24)` 作为 12 字节 nonce 走方式 7 的流程。

## 6/7/8 的 nonce 与布局

三者都把 `nonce ‖ ct ‖ tag16` 拼接后整体 Base64，放入 `data`；同时把 nonce 单独 Base64 后放入外层 `iv`。解密时从 `data` 头部按 12 或 24 字节切出 nonce，剩余部分至少 16 字节。认证失败返回空串；空明文只有在剩余长度恰好 16 字节（仅 tag）时才被接受。

## VIP 规则

加密方式 5、6、7、8 与签名方式 13、14 属于服务端会员能力。客户端不做会员判断，只按配置执行算法；没有权限时由服务端返回业务错误。因此本地能加密不代表账号已开通，联调时以服务端响应为准。

## 互操作测试向量

- AES-256-GCM：32 字节全零 key、12 字节全零 nonce、空明文、无 AAD，tag 为 `530f8afbc74536b9a963b4f1c4cb738b`。这是 NIST 向量，`tests/crypto_vectors.cpp` 已覆盖。方式 6 的 `data` 此时是 `base64(nonce ‖ tag)`。
- ChaCha20-Poly1305：仓库测试验证的是封装长度、往返和篡改拒绝，没有内置固定密文向量。需要跨实现比对时，请直接引用 RFC 8439 §2.8.2 的 key、nonce、AAD 与明文，按该标准计算并比较 tag，不要把未经验证的密文抄进文档或测试。
- BLAKE2b-256 与 SHA-256 的固定向量见 [签名方式](signature.md)，它们分别对应签名 14 的算法与密钥派生。

```bash
g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o crypto_vectors
./crypto_vectors
```

## 排查清单

1. `Crypto_Method` 是否与控制台“加密方式”完全一致，包括 1 与 5 这种都叫 AES-CBC 的编号。
2. 方式 6、7、8 是否按 key32 规则准备密钥；方式 1 是否误用了严格 Base64 的预期。
3. 方式 1 的服务端是否确实使用请求 IV 加密响应；跨语言实现时先固定 IV 做一次性比对。
4. 方式 3 的 UID 是否为空或含非 ASCII 字符。
5. 解密失败时先看错误码 1002，再看 `Crypto_Key` 是否被环境变量或配置中心改写。
