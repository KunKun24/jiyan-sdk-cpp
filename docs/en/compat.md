# Version Compatibility and Runtime Environment

This page summarizes the differences between SDK 1.8 and earlier versions, the server compatibility matrix, build requirements, threading, and resource limits. Older source trees are not in this repository, so statements about older internals rely on the README, CHANGELOG and current source. Where something cannot be confirmed, treat the source as authoritative.

## Version Matrix

| SDK version | Server requirement | Encryption | Signature | Notes |
| --- | --- | --- | --- | --- |
| 1.8 | 2026-11 or later | 0–8 | 0–14 | Current release, adds 6/7/8 and 14 |
| 1.7 | 2026-09 or later | 0–5 | 0–13 | Interface unchanged, still usable |
| 1.2–1.6 | Matching older servers | 0–5 | 0–13 | Compatible, upgrade recommended |

A server upgrade does not force old clients to upgrade. Encryption 6/7/8 and signature 14 require client 1.8 or later and a server that has enabled those algorithms. The 1.8 interface, parameters and return structures are unchanged; upgrading mainly means recompiling and running regression tests.

## Differences Between 1.8 and 1.2–1.7

- Encryption 6 (AES-256-GCM), 7 (ChaCha20-Poly1305) and 8 (XChaCha20-Poly1305) are new, as is signature 14 (keyed BLAKE2b-256).
- New implementations `AES256GCM.cpp`, `ChaCha20Poly1305.cpp`, `BLAKE2b`, plus fixed-vector tests in `tests/crypto_vectors.cpp`.
- New `JiYan_Key32` key handling for encryption 6/7/8 only: strict Base64 decode, use the decoded bytes when at least 16 bytes long, otherwise use the raw string, then pad with zeros or truncate to 32 bytes.
- Algorithm bounds extend from 5/13 to 8/14; out-of-range configuration raises `加密或签名方式编号非法`.
- Response validation is stricter: duplicate JSON keys, `rc` echo and cloud-control identity binding all produce 1003.
- Encryption 5–8 and signature 13–14 are server-side membership features; the client does not check permissions.

Heartbeat behavior is inconsistent between the documentation and the source. The README and CHANGELOG describe a 10-second interval, while `SDK_Heartbeat.cpp` waits with `std::chrono::minutes(interval)` and may overwrite the interval from `heartbeat_interval` (minutes) after a success. During an upgrade, trust the source and the integration test rather than the README alone.

## Server Compatibility

The server decrypts and verifies with the algorithm configured for the account, so client configuration must match exactly. In a mixed deployment, do not configure old clients with 6/7/8 or 14; they will receive 1002/1003 or a business error. After a server upgrade the old algorithms remain available; new algorithms work only for upgraded accounts with the required membership.

`api_url` must point at the API directory. `set_api_url` adds `http://` and a trailing `/`, and appends `/api` only when the URL has no path; a custom path is not rewritten to `/api/`. HTTPS is unsupported and explicitly rejected by the HTTP layer rather than silently downgraded.

## Build and Runtime Environment

| Item | Requirement |
| --- | --- |
| Language | C++17 |
| Android | NDK toolchain and JNI headers (`jni.h`, included directly by `SDK_Init.cpp`) |
| Linux desktop | g++ or clang++ with POSIX sockets, `dladdr`, `/proc/self/maps` |
| Build system | CMake 3.16+, `find_package(JNI REQUIRED)` |
| Third-party libraries | None: no OpenSSL, libsodium or libcurl; nlohmann/json is bundled |
| Network | Plain-socket HTTP/1.1 only; `http://` only, no TLS |

```bash
g++ -std=c++17 -O2 -c include/jiyan/SDK_Main.cpp -Iinclude/jiyan \
    -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux"
```

The source uses POSIX sockets, `dladdr` and `/proc/self/maps`, and requires JNI headers. Native Windows builds are outside the current support scope; a port would need replacements for those platform-specific parts. The repository does not pin an NDK version or Android API level; the toolchain must support C++17 and JNI, and the actual build result is authoritative.

## Threading Model

| Execution context | Start | Behavior |
| --- | --- | --- |
| Request thread | One detached thread per async request by default | Serialized by a recursive `RequestMutex`; `is_requesting` prevents re-entry |
| Heartbeat thread | Started after successful card validation | One global thread; fails twice in a row and locks the library, then exits |
| Cloud-control worker | Started by the first `更新()` with bindings | Polls by `poll_after`; retries in about 5 seconds on failure |
| Callbacks | Run on the request thread or the thread calling `更新()` | Never guaranteed to be the main thread; marshal UI work yourself |

`Enable_SingleThreaded()` makes requests synchronous, which suits consoles or hosts that cannot manage async lifetimes. Async APIs fill `bool&`/`std::string&` by reference, so those variables must outlive the request; a second request before completion is rejected or returns immediately. `Stop_Heartbeat()` joins the heartbeat thread. Cloud control must be shut down with `RemoteConfig::GetInstance().Shutdown()`, which has no Chinese wrapper.

## Resources and Limits

- Response body limit 8 MiB; request body limit 8 MiB; response headers, chunk lines and trailers share a 32 KiB budget.
- Connect timeout 15 seconds; send and receive share a 30-second total deadline.
- Cloud control allows at most 100 bindings, 100 pending ACKs and 4096 remembered command IDs; the polling interval is 1–30 seconds.
- The heartbeat interval is clamped to 1–1440 and used as minutes by the source.
- Size is dominated by the bundled json.hpp and the hash/AEAD implementations, and varies with compiler options and link-time stripping; the actual artifact is authoritative.

## Upgrade Advice

1. Upgrade the server first, then the client, and confirm the account has the required algorithms.
2. Keep the 1.8 call sites unchanged, recompile, and run `tests/crypto_vectors.cpp`.
3. Verify `Crypto_Method`, `Signature_Method`, `Crypto_Key` and `Signature_Key` against the server configuration.
4. Regress card validation, unbind, heartbeat, cloud control and cloud update, watching for 1002/1003.
5. Keep a rollback build. While old and new clients coexist, never deliver 6/7/8 or 14 to old clients.

Known limits: the SDK does not implement a downloader or UI and does not include the server; cloud-control `Shutdown` has no Chinese wrapper; membership is enforced only by the server. If behavior conflicts with this page, the repository source wins.
