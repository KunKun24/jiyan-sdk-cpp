# Encryption

Encryption sits in the protocol layer: the SDK serializes request parameters into a query string, encrypts it with `Crypto_Method`, and places the result in the `data` field of the outer JSON envelope. The response body is an encrypted string; the SDK decrypts it, verifies the signature, and checks `rc`. This page documents methods 0–8 as implemented in `SDK_TLS/JiYan_Crypto.cpp` and the per-algorithm files.

## Overview

| ID | Name | Key source | IV / nonce | Output format |
| --- | --- | --- | --- | --- |
| 0 | No encryption | None | None | Plain query string, empty `IV` |
| 1 | AES-256-CBC + PKCS7 | `Crypto_Key`, Base64-decoded | 16 random bytes, Base64 | `{"密文": base64(ct), "IV": base64(iv)}` |
| 2 | Custom Base64 | `Crypto_Key` as a 64-character alphabet | None | Custom alphabet with `=` padding |
| 3 | EnvProto | `Account_Uid` | None | Lowercase hex, two characters per byte |
| 4 | JiYan-Seal | Raw `Crypto_Key` bytes | 16 random bytes | base64url(nonce‖ct‖tag8), no padding |
| 5 | AES-256-CBC + HMAC | Raw `Crypto_Key` bytes | 16 random bytes | base64url(iv‖ct‖tag16), no padding |
| 6 | AES-256-GCM | `Crypto_Key` (key32 rule) | 12 random bytes | base64(nonce‖ct‖tag16) |
| 7 | ChaCha20-Poly1305 | `Crypto_Key` (key32 rule) | 12 random bytes | base64(nonce‖ct‖tag16) |
| 8 | XChaCha20-Poly1305 | `Crypto_Key` (key32 rule) | 24 random bytes | base64(nonce‖ct‖tag16) |

## Key Handling

Methods 6, 7 and 8 share `JiYan_Key32`:

1. Run a strict Base64 decode. Whitespace and omitted `=` are accepted; invalid characters, bad padding, and a length of 4n+1 are rejected.
2. If decoding succeeds and yields at least 16 bytes, use the decoded bytes; otherwise use the raw bytes of `Crypto_Key`.
3. Pad with zero bytes or truncate to exactly 32 bytes.

In other words, decode first, then pad or truncate. Method 1 is different: it uses a lenient Base64 decode (unknown characters become zero) and then pads or truncates to 32 bytes. Methods 4 and 5 pass the raw `Crypto_Key` string to HMAC-SHA256 without decoding. Method 2 treats `Crypto_Key` as the output alphabet and requires at least 64 bytes; a shorter value reads out of bounds. Method 3 uses `Account_Uid` and ignores `Crypto_Key`.

```cpp
App_Info app;
app.App_Pid = "YOUR-APP-ID";
app.Crypto_Method = 6;                    // 0-8, must match the console
app.Crypto_Key = "YOUR-CRYPTO-KEY";       // Methods 6/7/8 apply the key32 rule
JiYanSDK::init(account, app);             // account is a populated Account_Info
```

## Wire Format

The request envelope is always:

```cpp
{"developer_username": "...", "app_id": "...", "iv": "...", "data": "..."}
```

`data` is the encrypted query string. The response body is not a JSON envelope; it is the encrypted string itself. The SDK first checks for a `JiYan_Error` prefix, then decrypts with the same method. Method 1 decrypts with the IV sent in this request (the source passes `request.iv` back into the decrypt function), so the server must encrypt the response with the request IV rather than a fresh one. For methods 6, 7 and 8 the nonce sits at the head of `data`; the outer `iv` is only a Base64 copy of the same nonce.

## Method Details

Method 0: the query string is sent as-is and the response body is plain JSON. `IV` is an empty string.

Method 1: AES-256-CBC with PKCS7, always padded. The ciphertext is standard Base64 and is returned together with the IV. An invalid key or IV produces an empty ciphertext, after which the protocol layer raises `本地加密失败，请检查 Crypto_Key 与 Crypto_Method`.

Method 2: each 3-byte group is mapped to four characters taken from `Crypto_Key`, with Base64-style `=` padding at the end. Decoding strips trailing `=`, looks up each character in `Crypto_Key`, and fails on a missing character. This method exists for compatibility and provides no cryptographic strength.

Method 3: let `h = sum(byte(key[i]) * (i + 1)) % 100`. For plaintext byte i, compute `c = plain[i] XOR key[(i + h) % len(key)]`, then emit `(c + h + i) % 256` as lowercase hex. Decryption reverses the steps. An empty UID divides by zero; non-ASCII characters are treated differently by the encrypting and decrypting sides, so ASCII-only UIDs are recommended.

Method 4: the nonce is 16 random bytes. The keystream is a concatenation of 32-byte blocks, each `HMAC-SHA256(nonce ‖ be32(blockIndex), key)`. The ciphertext is plaintext XOR keystream, and the tag is the first 8 bytes of `HMAC-SHA256(nonce ‖ ct, key)`. The result is base64url without `=`; decryption requires at least 24 raw bytes.

Method 5: `encKey = HMAC-SHA256("jy1-enc", key)` and `macKey = HMAC-SHA256("jy1-mac", key)`. The IV is 16 random bytes, the ciphertext is AES-256-CBC-PKCS7, and the tag is the first 16 bytes of `HMAC-SHA256(iv ‖ ct, macKey)`. Output is base64url(iv‖ct‖tag16); decryption requires at least 32 raw bytes and verifies the tag before decrypting.

Method 6: AES-256-CTR with initial counter `J0 = nonce ‖ 00 00 00 01`. There is no AAD; GHASH covers only the ciphertext, with a length block of `len(A)=0 ‖ len(C)` in 64-bit big-endian. The tag is the 16-byte `GHASH ^ AES_K(J0)`, and the output is ct‖tag.

Method 7: IETF ChaCha20-Poly1305 with a 12-byte nonce. The one-time Poly1305 key comes from ChaCha20 block 0; data starts at counter 1; the tag is 16 bytes; output is ct‖tag.

Method 8: a 24-byte nonce. HChaCha20 derives a subkey from the key and the first 16 nonce bytes, then the flow of method 7 runs with the 12-byte nonce `00 00 00 00 ‖ nonce[16..24)`.

## Nonce and Layout for 6/7/8

All three concatenate `nonce ‖ ct ‖ tag16`, Base64-encode the result, and place it in `data`; the nonce alone is Base64-encoded into the outer `iv`. Decryption slices 12 or 24 bytes from the head of `data` and requires at least 16 bytes remaining. Authentication failure returns an empty string; an empty plaintext is accepted only when exactly 16 bytes (the tag alone) remain.

## VIP Rules

Encryption methods 5, 6, 7 and 8, together with signature methods 13 and 14, are server-side membership features. The client does not check membership; it simply runs the configured algorithm. Without permission the server returns a business error. Local encryption succeeding therefore does not prove that the account has access; trust the server response.

## Interoperability Vectors

- AES-256-GCM: 32 zero key bytes, 12 zero nonce bytes, empty plaintext, no AAD, tag `530f8afbc74536b9a963b4f1c4cb738b`. This is the NIST vector covered by `tests/crypto_vectors.cpp`. For method 6 the `data` value is `base64(nonce ‖ tag)` in this case.
- ChaCha20-Poly1305: the repository tests validate framing length, round-trip and tamper rejection, not a fixed ciphertext vector. For cross-implementation comparison, cite RFC 8439 §2.8.2 directly, use its key, nonce, AAD and plaintext, compute the tag, and compare. Do not copy an unverified ciphertext into documentation or tests.
- Fixed BLAKE2b-256 and SHA-256 vectors live in [Signature](signature.md); they correspond to the method 14 algorithm and its key derivation.

```bash
g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o crypto_vectors
./crypto_vectors
```

## Troubleshooting Checklist

1. Does `Crypto_Method` exactly match the console, including cases such as 1 and 5 that both use AES-CBC?
2. Are methods 6, 7 and 8 using the key32 rule, while method 1 is not being held to strict Base64 expectations?
3. Does the server really encrypt the method 1 response with the request IV? For a cross-language port, pin the IV and compare once.
4. Is the method 3 UID empty, or does it contain non-ASCII characters?
5. On a decryption failure, check error code 1002 first, then verify that `Crypto_Key` was not rewritten by an environment variable or config center.
