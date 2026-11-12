/* --------------------------------- 关于 -------------------------------------

                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_CRYPTO_CPP
#define JIYAN_CRYPTO_CPP

#include "Crypto/AES256CBC.cpp"
#include "Crypto/base64自定义编码集.cpp"
#include "Crypto/JiYanEnvProto加解密.cpp"
#include "Crypto/JiYanSeal.cpp"
#include "Crypto/JiYanAesHmac.cpp"
#include "Crypto/AES256GCM.cpp"
#include "Crypto/ChaCha20Poly1305.cpp"

#include <cstdio>
#include <random>

/* ---------- 1.8 新增算法的公共辅助（与 PHP AppCrypto 逐字节一致） ---------- */

/* base64 严格解码：拒绝非法字符；允许省略 '=' 填充 */
static bool JiYan_B64_Decode_Strict(const std::string& in, std::vector<unsigned char>& out) {
    std::string s;
    for (size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        s.push_back(c);
    }
    if (s.size() % 4 == 1) return false;

    size_t pad = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '=') {
            ++pad;
            if (i + 1 < s.size()) return false;
        } else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '+' || c == '/')) {
            return false;
        }
    }
    if (pad > 2) return false;
    out = AES256_base64_decode(s);
    return true;
}

/* 与服务端 key32 一致：合法 base64 且解出 >=16 字节则用解码结果，否则用原文；补零/截断到 32 */
static std::vector<unsigned char> JiYan_Key32(const std::string& key) {
    std::vector<unsigned char> raw;
    std::vector<unsigned char> decoded;
    if (JiYan_B64_Decode_Strict(key, decoded) && decoded.size() >= 16) {
        raw = decoded;
    } else {
        raw.assign(key.begin(), key.end());
    }
    raw.resize(32, 0);
    return raw;
}

static std::string JiYan_Random_Bytes(size_t n) {
    std::string out;
    out.resize(n);
    FILE* fp = fopen("/dev/urandom", "rb");
    if (fp != nullptr) {
        const size_t got = fread(&out[0], 1, n, fp);
        fclose(fp);
        if (got == n) return out;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    for (size_t i = 0; i < n; ++i) out[i] = static_cast<char>(dis(gen));
    return out;
}

/* 加密 6/7/8：nonce 随机，输出 base64(nonce || ct || tag) */
static nlohmann::json JiYan_Crypto_Aead_Encryption(const std::string& content,
                                                   const std::string& key,
                                                   int method) {
    const size_t nonceLen = (method == 8) ? 24 : 12;
    const std::string nonce = JiYan_Random_Bytes(nonceLen);
    const std::vector<unsigned char> k = JiYan_Key32(key);

    std::vector<unsigned char> body;
    if (method == 6) {
        body = JiYan_AES256GCM_Seal(k.data(),
                                    reinterpret_cast<const unsigned char*>(nonce.data()),
                                    reinterpret_cast<const unsigned char*>(content.data()),
                                    content.size(), 12);
    } else if (method == 7) {
        body = JiYan_ChaCha20Poly1305_Seal(k.data(),
                                           reinterpret_cast<const unsigned char*>(nonce.data()),
                                           reinterpret_cast<const unsigned char*>(content.data()),
                                           content.size());
    } else {
        body = JiYan_XChaCha20Poly1305_Seal(k.data(),
                                            reinterpret_cast<const unsigned char*>(nonce.data()),
                                            reinterpret_cast<const unsigned char*>(content.data()),
                                            content.size());
    }

    std::vector<unsigned char> payload(nonce.begin(), nonce.end());
    payload.insert(payload.end(), body.begin(), body.end());

    return {{"密文", AES256_base64_encode_string(payload)},
            {"IV", AES256_base64_encode_string(std::vector<unsigned char>(nonce.begin(), nonce.end()))}};
}

/* 解密 6/7/8：从密文头部取 nonce，认证失败返回空 */
static std::string JiYan_Crypto_Aead_Decryption(const std::string& content,
                                                const std::string& key,
                                                int method) {
    std::vector<unsigned char> payload;
    if (!JiYan_B64_Decode_Strict(content, payload)) return "";

    const size_t nonceLen = (method == 8) ? 24 : 12;
    if (payload.size() < nonceLen + 16) return "";

    const std::vector<unsigned char> k = JiYan_Key32(key);
    const unsigned char* nonce = payload.data();
    const unsigned char* body = payload.data() + nonceLen;
    const size_t bodyLen = payload.size() - nonceLen;

    std::vector<unsigned char> plain;
    if (method == 6) {
        plain = JiYan_AES256GCM_Open(k.data(), nonce, body, bodyLen);
    } else if (method == 7) {
        plain = JiYan_ChaCha20Poly1305_Open(k.data(), nonce, body, bodyLen);
    } else {
        plain = JiYan_XChaCha20Poly1305_Open(k.data(), nonce, body, bodyLen);
    }

    if (plain.empty() && bodyLen != 16) return "";
    return std::string(plain.begin(), plain.end());
}

nlohmann::json JiYan_Crypto_Encryption_Content(const std::string& content,
                                              const JiYanSDK::RuntimeSnapshot& snapshot) {
    const auto& app = snapshot.app;
    switch (app.Crypto_Method) {
    case 0: return {{"密文", content}, {"IV", ""}};
    case 1: return JiYan_Crypto_AES256CBC_Encryption(content, app.Crypto_Key, AES256_GenerateIV());
    case 2: return JiYan_Crypto_Base64_Encryption(content, app.Crypto_Key);
    case 3: return JiYan_Crypto_EnvProto_Encryption(content, snapshot.account.Account_Uid);
    case 4: return JiYan_Crypto_Seal_Encryption(content, app.Crypto_Key);
    case 5: return JiYan_Crypto_AesHmac_Encryption(content, app.Crypto_Key);
    case 6: return JiYan_Crypto_Aead_Encryption(content, app.Crypto_Key, 6);
    case 7: return JiYan_Crypto_Aead_Encryption(content, app.Crypto_Key, 7);
    case 8: return JiYan_Crypto_Aead_Encryption(content, app.Crypto_Key, 8);
    default: return {{"密文", ""}, {"IV", ""}};
    }
}

std::string JiYan_Crypto_Decryption_Content(const std::string& content,
                                           const JiYanSDK::RuntimeSnapshot& snapshot,
                                           const std::string& request_iv) {
    const auto& app = snapshot.app;
    switch (app.Crypto_Method) {
    case 0: return content;
    case 1: return JiYan_Crypto_AES256CBC_Decryption(content, app.Crypto_Key, request_iv);
    case 2: return JiYan_Crypto_Base64_Decryption(content, app.Crypto_Key);
    case 3: return JiYan_Crypto_EnvProto_Decryption(content, snapshot.account.Account_Uid);
    case 4: return JiYan_Crypto_Seal_Decryption(content, app.Crypto_Key);
    case 5: return JiYan_Crypto_AesHmac_Decryption(content, app.Crypto_Key);
    case 6: return JiYan_Crypto_Aead_Decryption(content, app.Crypto_Key, 6);
    case 7: return JiYan_Crypto_Aead_Decryption(content, app.Crypto_Key, 7);
    case 8: return JiYan_Crypto_Aead_Decryption(content, app.Crypto_Key, 8);
    default: return "";
    }
}

thread_local std::string JiYan_Compatibility_IV;
nlohmann::json JiYan_Crypto_Encryption_Content(std::string content) {
    auto result = JiYan_Crypto_Encryption_Content(content, JiYanSDK::GetRuntimeSnapshot());
    JiYan_Compatibility_IV = result.value("IV", std::string());
    return result;
}
std::string JiYan_Crypto_Decryption_Content(std::string content) {
    return JiYan_Crypto_Decryption_Content(content, JiYanSDK::GetRuntimeSnapshot(), JiYan_Compatibility_IV);
}
#endif
