# Error Codes and Messages

The SDK reports errors through `JiYanSDK_Result::Data`: `ok` tells whether the business call succeeded, `code` is the numeric code, `message` is displayable text, and `raw` holds the decrypted response body. `JiYan_Last_Request_Ok` is a thread-local flag updated when an asynchronous request finishes. Messages originate in the protocol layer, the HTTP layer, and the business wrappers; `SDK_Result.cpp` itself produces no error strings.

## Error Codes

| code | Meaning | Origin | Handling |
| --- | --- | --- | --- |
| 0 | Success | Protocol layer saw `success=true` | Read the business fields in `Data` |
| -1 | No response | `Response` default; transport failure | Check network, DNS, port, `api_url`, HTTP status |
| 1002 | Response decryption failed | `Decode` produced empty plaintext | Verify `Crypto_Method` and `Crypto_Key` |
| 1003 | Response verification failed | Signature, `rc`, or cloud identity binding | Check signing config, proxies/caches, identity changes |
| 9999 | Client exception, `JiYan_Error`, or missing business code | Exception handler, error prefix, failed response with `code=0` | Read `message` and classify it |
| Other | Server business code | `code` in the response JSON | Handle according to the business meaning, not as transport failure |

When `ok=false` and `code=-1`, there is usually no usable HTTP status at all. When `code=9999`, `message` is often the raw exception text. If the server reports failure with `code=0`, the SDK forces the code to 9999.

## The JiYan_Error Prefix

If the response body is plain text beginning with `JiYan_Error`, the SDK skips decryption, uses everything after the first 11 characters as `message`, and sets `code` to 9999. The prefix only appears in the read direction; the client never emits it. It usually comes from a gateway, WAF, PHP fatal error, or maintenance page, meaning the request never reached normal protocol handling. Inspect the server and reverse-proxy logs rather than only the encryption settings.

## Local Configuration Errors

These are raised before any request is sent and surface as `code=9999`:

| message | Cause |
| --- | --- |
| `加密或签名方式编号非法` | `Crypto_Method` outside 0–8 or `Signature_Method` outside 0–14 |
| `Signature_Key 未配置` | Signature method is not 0 but the key is empty |
| `Crypto_Key 未配置` | Encryption method is 1, 4, 5, 6, 7 or 8 but the key is empty |
| `本地签名失败` | The signature implementation returned empty, usually due to key or encoding |
| `本地加密失败，请检查 Crypto_Key 与 Crypto_Method` | Encryption returned empty, common with an invalid method 1 key or IV |
| `请求参数必须是字符串或整数，数组应先编码成JSON字符串` | An array or object was passed as a parameter |
| `此客户端只允许卡密、同行识别和云更新接口` | `set_php` points at an unsupported endpoint |
| `新 HTTP 客户端包不开放管理操作，请在 HTTPS 主站后台操作` | A management API such as `GetFileList` was called |

## Protocol and Response Errors

| message | Cause | Suggestion |
| --- | --- | --- |
| `响应解密失败（Crypto_Key / Crypto_Method 配置不匹配）` | Ciphertext cannot be opened | Both sides must agree on algorithm, key handling and IV reuse |
| `响应签名校验失败` | Recomputed signature differs | Check `Signature_Method`, `Signature_Key` and the canonical string |
| `响应校验失败（rc 不匹配或缺失）` | Response does not belong to this request | Disable caching/replay proxies; confirm the server echoes this request's `rc` |
| `云控响应身份绑定不匹配` | Cloud-control nonce, scope, app, card or device differs | Check whether account, card or device changed during the request |
| `响应结构非法` | JSON lacks `success`/`message`/`data` or has wrong types | Check whether the server returned a non-protocol page |
| `响应解析失败：...` | Business-layer re-parse failed | Combine `raw` and `message` |
| `网络请求失败` | Transport failed without a more specific error | Continue with the HTTP errors below |

## HTTP Errors

HTTP-layer failures use `code=-1` and a message of the form `HTTP <status> - <hint>`. Common statuses:

| Status | Hint summary | Typical cause |
| --- | --- | --- |
| 400 | Bad request format, check JSON/fields | Request body rewritten or fields missing |
| 401 | Unauthorized, account/password/token invalid or expired | Wrong account information |
| 402 | Payment required, insufficient balance or not enabled | Membership feature not enabled |
| 403 | Forbidden, permissions/IP/WAF | Firewall or origin restriction |
| 404 | Endpoint not found, check that `api_url` ends with `/api/` | Missing `/api/` or wrong endpoint filename |
| 405 | POST only | The endpoint was accessed with GET |
| 408 | Request timeout | Slow network or busy server |
| 413 | Payload too large | Too many parameters in one batch; split the request |
| 429 | Rate limited | Lower the request rate |
| 500 | Internal server error | Backend fault; inspect server logs |
| 502 | Bad gateway, PHP-FPM down/restarting/timing out | Backend process unavailable |
| 503 | Service unavailable, maintenance or pool exhausted | Retry later |
| 504 | Gateway timeout | Backend processing is too slow |

Transport errors can also be reported directly: `HTTP 网络操作超时`, `HTTP DNS/地址解析失败：...`, `HTTP 连接失败：...`, `HTTP 发送失败：...`, `HTTP 接收失败：...`, `HTTP 响应正文超过 8 MiB`, `HTTP 响应头、分块行或尾部超过 32 KiB 限制`, and similar text. HTTPS URLs are explicitly rejected with `纯 Socket HTTP 不支持 HTTPS，禁止降级；请使用 http:// 地址`.

## Business and Cloud Update Errors

Card, peer-detection and announcement failures come from the server with a `code` and `message`, passed through unchanged. Cloud update also produces local validation messages, including:

| message | Cause |
| --- | --- |
| `文件名或本地 MD5 无效` | MD5 is not 32 hex characters or the filename is empty |
| `本地 SO 文件无法读取或未加载，不能进行 MD5 检查` | The SO cannot be found or read via `/proc/self/maps` |
| `下载文件名、摘要或票据不匹配，发布可能已变更，请重新检查版本` | The release changed while obtaining the URL |
| `下载文件 MD5 不匹配，未提交回执` | The written file is corrupt or was modified |
| `回执响应未确认下载完成` | Server returned `confirmed != 1` |
| `已有异步请求正在进行，请稍后重试` | Concurrent call to a compatibility async API |

Cloud-control binding failures do not set `Data.code`; the binding function returns `false` and stores the reason in `最后错误()`, for example an invalid key, a duplicate binding, a duplicate address, or more than 100 bindings.

## Troubleshooting Order

1. Read `Data.ok`, `Data.code` and `Data.message` first, then check whether `raw` is empty.
2. `code=-1`: network and HTTP. `1002`: encryption. `1003`: signature, `rc` and identity. `9999`: read the message.
3. An empty device ID breaks validation and cloud control. Confirm that `set_device_id` is called before `init` and that the value is stable.
4. Two consecutive heartbeat failures lock the library and `is_locked()` becomes true. After fixing the network, validate the card again.
5. Never log full keys, card keys or account credentials. When comparison is needed, log only lengths and digests.

## Minimal Error Handling

```cpp
bool ok = false;
std::string message;
JiYanSDK_API::卡密验证::使用卡密("YOUR-CARD-KEY", ok, message);
if (!ok) {
    const auto& r = JiYanSDK_Result::Data;
    // r.code: -1 / 1002 / 1003 / 9999 or a server business code
    // r.message: displayable; r.raw is the decrypted response body
    std::cerr << "code=" << r.code << ", message=" << r.message << "\n";
}
```
