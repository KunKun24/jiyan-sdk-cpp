// 极验云 C++ SDK · 算法自测
// 只依赖 SDK 自带实现，不联网、不需要 Java 环境。
// 构建：g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o crypto_vectors

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "json.hpp"
#include "SDK_TLS/Signature/SHA256.cpp"
#include "SDK_TLS/Crypto/AES256CBC.cpp"
#include "SDK_TLS/Crypto/AES256GCM.cpp"
#include "SDK_TLS/Crypto/ChaCha20Poly1305.cpp"
#include "SDK_TLS/Signature/BLAKE2b.cpp"

static int g_pass = 0;
static int g_fail = 0;

static std::string hex(const std::vector<unsigned char>& v) {
    static const char* k = "0123456789abcdef";
    std::string s;
    s.reserve(v.size() * 2);
    for (unsigned char c : v) {
        s.push_back(k[c >> 4]);
        s.push_back(k[c & 15]);
    }
    return s;
}

static std::string hex_to_raw(const std::string& h) {
    std::string out;
    out.resize(h.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        auto nib = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return (unsigned char)(c - '0');
            if (c >= 'a' && c <= 'f') return (unsigned char)(c - 'a' + 10);
            return (unsigned char)(c - 'A' + 10);
        };
        out[i] = (char)((nib(h[i * 2]) << 4) | nib(h[i * 2 + 1]));
    }
    return out;
}

static void check(const char* name, bool ok) {
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) ++g_pass; else ++g_fail;
}

int main() {
    // 1. BLAKE2b-256 标准向量（无密钥，"abc"）
    check("BLAKE2b-256(\"abc\")",
          JiYan_Blake2b_256_Keyed_Hex("abc", "") ==
              "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319");

    // 2. SHA-256 标准向量（签名 14 的密钥派生要用）
    check("SHA-256(\"abc\")",
          SHA256::hash("abc") ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // 3. AES-256-GCM 空明文 NIST 向量：key/nonce 全 0，tag 固定
    {
        unsigned char key[32] = {0};
        unsigned char nonce[12] = {0};
        unsigned char zero = 0;
        auto body = JiYan_AES256GCM_Seal(key, nonce, &zero, 0, 12);
        check("AES-256-GCM empty-plaintext tag",
              body.size() == 16 &&
                  hex(body) == "530f8afbc74536b9a963b4f1c4cb738b");
    }

    // 4. ChaCha20-Poly1305 往返 + 篡改拒绝
    {
        unsigned char key[32];
        unsigned char nonce[12];
        for (int i = 0; i < 32; ++i) key[i] = (unsigned char)i;
        for (int i = 0; i < 12; ++i) nonce[i] = (unsigned char)(0xa0 + i);
        const std::string msg = "hello JiYan Cloud";
        auto body = JiYan_ChaCha20Poly1305_Seal(
            key, nonce, reinterpret_cast<const unsigned char*>(msg.data()), msg.size());
        check("ChaCha20-Poly1305 size", body.size() == msg.size() + 16);

        auto plain = JiYan_ChaCha20Poly1305_Open(key, nonce, body.data(), body.size());
        check("ChaCha20-Poly1305 roundtrip",
              std::string(plain.begin(), plain.end()) == msg);

        std::vector<unsigned char> bad = body;
        bad[bad.size() - 1] ^= 0x01;
        auto plain2 = JiYan_ChaCha20Poly1305_Open(key, nonce, bad.data(), bad.size());
        check("ChaCha20-Poly1305 tamper rejected", plain2.empty());
    }

    // 5. XChaCha20-Poly1305 往返 + 篡改拒绝
    {
        unsigned char key[32];
        unsigned char nonce[24];
        for (int i = 0; i < 32; ++i) key[i] = (unsigned char)(0x40 + i);
        for (int i = 0; i < 24; ++i) nonce[i] = (unsigned char)(0x10 + i);
        const std::string msg = "xchacha 24-byte nonce";
        auto body = JiYan_XChaCha20Poly1305_Seal(
            key, nonce, reinterpret_cast<const unsigned char*>(msg.data()), msg.size());
        auto plain = JiYan_XChaCha20Poly1305_Open(key, nonce, body.data(), body.size());
        check("XChaCha20-Poly1305 roundtrip",
              std::string(plain.begin(), plain.end()) == msg);

        std::vector<unsigned char> bad = body;
        bad[0] ^= 0x80;
        auto plain2 = JiYan_XChaCha20Poly1305_Open(key, nonce, bad.data(), bad.size());
        check("XChaCha20-Poly1305 tamper rejected", plain2.empty());
    }

    // 6. 签名 14 的完整口径：key = SHA-256(sign_key) 的原始 32 字节
    {
        const std::string signKey = "1638006845-JiYan";
        const std::string key32 = hex_to_raw(SHA256::hash(signKey));
        const std::string got = JiYan_Blake2b_256_Keyed_Hex("payload", key32);
        check("signature 14 format (64 lowercase hex)",
              got.size() == 64 && got.find_first_not_of("0123456789abcdef") == std::string::npos);
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
