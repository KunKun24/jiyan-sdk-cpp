# 签名方式

签名由协议层统一计算：SDK 先构造 canonical 串，用 `Signature_Method` 指定的算法和 `Signature_Key` 生成 `signature` 字段，服务端用同样规则复算。响应也带签名，SDK 在解密后、返回业务数据前校验。本页公式以 `SDK_TLS/JiYan_Signature.cpp` 与 `SDK_Protocol.cpp` 为准。

## 签名列表

| ID | 算法 | 参与签名的消息 | 输出 |
| --- | --- | --- | --- |
| 0 | 不签名 | 无 | 空字符串 |
| 1 | MD5 | canonical + key | 32 位小写 hex |
| 2 | SHA-512 | canonical + key | 128 位小写 hex |
| 3 | CRC32 | canonical + key | 8 位小写 hex |
| 4 | SHA-256 | canonical + key | 64 位小写 hex |
| 5 | RIPEMD-160 | canonical + key | 40 位小写 hex |
| 6 | SHA-1 | canonical + key | 40 位小写 hex |
| 7 | SHA-224 | canonical + key | 56 位小写 hex |
| 8 | SHA-384 | canonical + key | 96 位小写 hex |
| 9 | SHA3-512 | canonical + key | 128 位小写 hex |
| 10 | SHA3-256 | canonical + key | 64 位小写 hex |
| 11 | SHA3-224 | canonical + key | 56 位小写 hex |
| 12 | HMAC-SHA256 | canonical（密钥单独传入） | 64 位小写 hex |
| 13 | 双 HMAC-SHA256 | canonical（密钥单独传入） | 81 字符 |
| 14 | BLAKE2b-256（带密钥） | canonical（密钥为派生值） | 64 位小写 hex |

方式 1–11 是把密钥直接拼在消息尾部再摘要，不是 HMAC。编号也不按强度递增：1 是 MD5，2 是 SHA-512，3 是 CRC32，配置时务必按表核对，不要凭编号猜算法。

## canonical 串

参与签名的字段包括 `action`、`app_id`、`signature_version`（固定 `2`）、`signature_scope`、`timestamp`、`nonce`、`rc`，以及业务参数（如 `card_key`、`device_id`、`user_uid`）。`signature` 自身先被移除，签名完成后再加回。

构造规则：

1. 按字段名的字节序升序排列（源码使用 `std::map`）。
2. 写入固定头：`jy-req-v2`、换行、`scope`、换行、`app_id`、换行、`2`、换行。
3. 每个字段写成 `len(key):key=len(value):value`，末尾再加一个换行。
4. 长度是 UTF-8 字节数，不是字符数；值不做 URL 编码。

标量转换规则：字符串原样；布尔 `true` 记为 `1`，`false` 记为空串；`null` 记为空串；整数取十进制；数组和对象直接抛错，要求先编码成 JSON 字符串。

```text
jy-req-v2
key
10001
2
6:action=8:use_card
6:app_id=5:10001
8:card_key=13:TEST-CARD-KEY
9:device_id=7:dev-001
5:nonce=6:abcdef
2:rc=6:123456
16:signature_scope=3:key
17:signature_version=1:2
9:timestamp=10:1700000000
```

上例按字典序排列，字段之间以换行分隔，最后一行同样带换行。`scope` 取 `key`、`user` 或 `cloud`；云控同步使用 `cloud_control`，响应还会额外绑定 nonce、app_id、card_key 与 device_id。

## 12、13、14 的精确构造

方式 12 对 canonical 做 HMAC-SHA256，密钥是 `Signature_Key`，输出小写 hex：

```cpp
std::string h = Hmac_Sha256_Hex(canonical, sign_key);
```

方式 13 在 12 的基础上再算一次带前缀的 HMAC，取前 16 个 hex 字符，用点号连接，总长 81：

```cpp
std::string h1 = Hmac_Sha256_Hex(canonical, sign_key);
std::string h2 = Hmac_Sha256_Hex("jy1" + canonical + sign_key, sign_key);
std::string signature = h1 + "." + h2.substr(0, 16);
```

方式 14 的密钥不是 `Signature_Key` 本身，而是它的 SHA-256 原始字节：

```cpp
// key32 是 32 个原始字节，不是 64 个 hex 字符
std::string key32 = Hex_To_Raw(Sha256_Hex(sign_key));
std::string signature = Blake2b_256_Keyed_Hex(key32, canonical);
```

`Sha256_Hex` 输出 64 位小写 hex，`Hex_To_Raw` 再转回 32 字节，作为 BLAKE2b-256 的 key。最终结果仍是 64 位小写 hex。若把 64 字符的 hex 直接当 key，服务端必然复算不一致。

## 空 sign_key 的行为

方式 0 不签名：请求不带 `signature`，响应也不校验签名，适合本地调试，不建议生产使用。

方式 1–14 在协议层要求 `Signature_Key` 非空，否则 `Encode` 直接抛出 `Signature_Key 未配置`，请求不会发出。方式 14 的底层函数对空 key 返回空串，但由于上层已拦截，正常调用路径不会走到。密钥按原样使用，不做 trim；首尾空格、换行都算密钥的一部分。

## 响应签名

响应验签的消息为：`success` 的字符串形式（`true` 或 `false`）、`message`、`data` 的原始 JSON 字节，依次拼接。`data` 必须使用响应原文中的字节，不能解析成对象后再序列化，否则字段顺序和转义会变化。SDK 用 `RawValueOf` 提取原文，再以同一签名函数和 `Signature_Key` 复算，用恒定时间比较。

`Signature_Method` 为 0 时跳过响应验签；其余情况下不一致返回错误码 1003，文案为 `响应签名校验失败`。验签通过后还会校验 `rc`，云控响应另有身份绑定校验。

## 调试方法

1. 把 canonical 串按字节打印出来，连同每个字段的长度一起对照，最常见的问题是长度按字符数算、漏掉末尾换行、布尔值写成 `true`。
2. 确认 `signature_version` 是字符串 `2`，`timestamp` 是十进制秒，`nonce` 和 `rc` 都是 24 字符。
3. 对比两端使用的算法编号，而不是算法名称；尤其注意 1 与 4、2 与 8 的错配。
4. 检查密钥来源是否被配置文件、环境变量或换行符改写。跨语言实现时先固定 canonical 串，只比较摘要。
5. 方式 14 先单独验证 `SHA256("abc")` 与 `BLAKE2b-256("abc")`，再验证带密钥输出；`tests/crypto_vectors.cpp` 提供了固定向量。

```bash
g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o crypto_vectors
./crypto_vectors
```

响应返回 1003 时，先区分是签名、`rc` 还是云控身份绑定失败；三者文案不同，但都归为 1003。若服务端日志能打印它复算的 canonical，直接逐字节比对即可定位。
