# Signature

The protocol layer computes every signature. The SDK builds a canonical string, signs it with the algorithm selected by `Signature_Method` and the configured `Signature_Key`, and adds the result as the `signature` field. The server recomputes it with the same rules. Responses are signed too: the SDK verifies the signature after decryption and before returning business data. The formulas below follow `SDK_TLS/JiYan_Signature.cpp` and `SDK_Protocol.cpp`.

## Signature Methods

| ID | Algorithm | Signed message | Output |
| --- | --- | --- | --- |
| 0 | None | None | Empty string |
| 1 | MD5 | canonical + key | 32 lowercase hex |
| 2 | SHA-512 | canonical + key | 128 lowercase hex |
| 3 | CRC32 | canonical + key | 8 lowercase hex |
| 4 | SHA-256 | canonical + key | 64 lowercase hex |
| 5 | RIPEMD-160 | canonical + key | 40 lowercase hex |
| 6 | SHA-1 | canonical + key | 40 lowercase hex |
| 7 | SHA-224 | canonical + key | 56 lowercase hex |
| 8 | SHA-384 | canonical + key | 96 lowercase hex |
| 9 | SHA3-512 | canonical + key | 128 lowercase hex |
| 10 | SHA3-256 | canonical + key | 64 lowercase hex |
| 11 | SHA3-224 | canonical + key | 56 lowercase hex |
| 12 | HMAC-SHA256 | canonical (key passed separately) | 64 lowercase hex |
| 13 | Double HMAC-SHA256 | canonical (key passed separately) | 81 characters |
| 14 | Keyed BLAKE2b-256 | canonical (derived key) | 64 lowercase hex |

Methods 1–11 append the key to the message and then hash it; they are not HMAC. The numbering is not ordered by strength either: 1 is MD5, 2 is SHA-512, 3 is CRC32. Always match the number against this table rather than guessing from the name.

## Canonical String

The signed fields are `action`, `app_id`, `signature_version` (always `2`), `signature_scope`, `timestamp`, `nonce`, `rc`, plus business parameters such as `card_key`, `device_id` and `user_uid`. The `signature` field is removed first and appended after signing.

Construction rules:

1. Sort fields by byte order of the field name (the source uses `std::map`).
2. Write the fixed header: `jy-req-v2`, newline, `scope`, newline, `app_id`, newline, `2`, newline.
3. Write each field as `len(key):key=len(value):value`, followed by a newline.
4. Lengths are UTF-8 byte counts, not character counts; values are not URL-encoded.

Scalar conversion: strings pass through; boolean `true` becomes `1` and `false` becomes an empty string; `null` becomes an empty string; integers use decimal notation; arrays and objects throw, so encode them as JSON strings first.

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

The example is in dictionary order, with a newline between fields and a trailing newline after the last one. `scope` is `key`, `user` or `cloud`; cloud-control synchronization uses `cloud_control`, and its response additionally binds nonce, app_id, card_key and device_id.

## Exact Construction for 12, 13 and 14

Method 12 computes HMAC-SHA256 over the canonical string with `Signature_Key` and returns lowercase hex:

```cpp
std::string h = Hmac_Sha256_Hex(canonical, sign_key);
```

Method 13 adds a second HMAC with a prefix, takes the first 16 hex characters, and joins the two parts with a dot for a total of 81 characters:

```cpp
std::string h1 = Hmac_Sha256_Hex(canonical, sign_key);
std::string h2 = Hmac_Sha256_Hex("jy1" + canonical + sign_key, sign_key);
std::string signature = h1 + "." + h2.substr(0, 16);
```

Method 14 does not use `Signature_Key` directly; it derives the key with SHA-256:

```cpp
// key32 is 32 raw bytes, not the 64-character hex string
std::string key32 = Hex_To_Raw(Sha256_Hex(sign_key));
std::string signature = Blake2b_256_Keyed_Hex(key32, canonical);
```

`Sha256_Hex` returns 64 lowercase hex characters, and `Hex_To_Raw` converts them back to 32 bytes for keyed BLAKE2b-256. The final result is again 64 lowercase hex characters. Passing the 64-character hex string as the key will never match the server.

## Empty sign_key

Method 0 signs nothing: the request carries no `signature` and the response signature is skipped. It is useful for local debugging and not recommended in production.

For methods 1–14 the protocol layer requires a non-empty `Signature_Key`; otherwise `Encode` throws `Signature_Key 未配置` and no request is sent. The method 14 helper itself returns an empty string for an empty key, but the upper layer normally prevents that path. The key is used verbatim: leading or trailing spaces and newlines are part of it and are never trimmed.

## Response Signature

The response signature covers the string form of `success` (`true` or `false`), `message`, and the raw JSON bytes of `data`, concatenated in that order. `data` must be the bytes from the response body, not a parsed object re-serialized by the client, because field order and escaping would change. The SDK extracts the original bytes with `RawValueOf`, recomputes the signature with the same function and key, and compares in constant time.

When `Signature_Method` is 0 the response check is skipped. Otherwise a mismatch returns code 1003 with the message `响应签名校验失败`. After verification the `rc` is also checked, and cloud-control responses have an additional identity binding check.

## Debugging

1. Print the canonical string byte by byte and compare every declared length. The most common mistakes are counting characters instead of bytes, dropping the trailing newline, and writing `true` instead of `1`.
2. Confirm that `signature_version` is the string `2`, `timestamp` is decimal seconds, and `nonce` and `rc` are both 24 characters.
3. Compare algorithm numbers rather than names, especially the 1/4 and 2/8 pairs.
4. Check whether the key was rewritten by a config file, an environment variable, or an embedded newline. For a cross-language port, pin the canonical string first and compare digests only.
5. For method 14, verify `SHA256("abc")` and `BLAKE2b-256("abc")` separately before checking keyed output. `tests/crypto_vectors.cpp` contains fixed vectors.

```bash
g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o crypto_vectors
./crypto_vectors
```

When the server returns 1003, first determine whether the signature, the `rc`, or the cloud-control identity binding failed. The messages differ but share the same code. If the server log can print the canonical string it recomputed, compare the two byte by byte; that usually locates the problem immediately.
