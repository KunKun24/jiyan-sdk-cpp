/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SEAL_CPP
#define JIYAN_SEAL_CPP

#include <string>
#include <vector>
#include <random>
#include <cstdio>

std::string JiYan_HMAC_SHA256_Raw(const std::string &msg, const std::string &key);
bool JiYan_Secure_Equal(const std::string &a, const std::string &b);

static std::string JiYan_Random16()
{
    std::string out;
    out.resize(16);
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp != NULL)
    {
        const size_t got = fread(&out[0], 1, 16, fp);
        fclose(fp);
        if (got == 16) return out;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    for (int i = 0; i < 16; ++i) out[i] = static_cast<char>(dis(gen));
    return out;
}

static std::string JiYan_B64Url_Encode(const std::string &raw)
{
    std::string s = AES256_base64_encode_string(
        std::vector<unsigned char>(raw.begin(), raw.end()));
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '+') s[i] = '-';
        else if (s[i] == '/') s[i] = '_';
    }
    while (!s.empty() && s.back() == '=') s.pop_back();
    return s;
}

static std::string JiYan_B64Url_Decode(const std::string &in)
{
    std::string s = in;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '-') s[i] = '+';
        else if (s[i] == '_') s[i] = '/';
    }
    while (s.size() % 4 != 0) s.push_back('=');
    std::vector<unsigned char> raw = AES256_base64_decode(s);
    return std::string(raw.begin(), raw.end());
}

static std::string JiYan_Seal_Keystream(size_t len, const std::string &key, const std::string &nonce)
{
    const size_t blocks = (len + 31) / 32;
    std::string ks;
    ks.reserve(blocks * 32);
    for (size_t i = 0; i < blocks; ++i)
    {
        std::string msg = nonce;
        msg.push_back(static_cast<char>((i >> 24) & 0xFF));
        msg.push_back(static_cast<char>((i >> 16) & 0xFF));
        msg.push_back(static_cast<char>((i >> 8) & 0xFF));
        msg.push_back(static_cast<char>(i & 0xFF));
        ks += JiYan_HMAC_SHA256_Raw(msg, key);
    }
    return ks;
}

nlohmann::json JiYan_Crypto_Seal_Encryption(const std::string &plain, const std::string &key)
{
    nlohmann::json result;
    result["密文"] = "";
    result["IV"] = "";

    if (key.empty()) return result;

    const std::string nonce = JiYan_Random16();
    const std::string ks = JiYan_Seal_Keystream(plain.size(), key, nonce);

    std::string ct;
    ct.reserve(plain.size());
    for (size_t i = 0; i < plain.size(); ++i)
    {
        ct.push_back(static_cast<char>(
            static_cast<unsigned char>(plain[i]) ^ static_cast<unsigned char>(ks[i])));
    }

    const std::string tag = JiYan_HMAC_SHA256_Raw(nonce + ct, key).substr(0, 8);

    result["密文"] = JiYan_B64Url_Encode(nonce + ct + tag);
    return result;
}

std::string JiYan_Crypto_Seal_Decryption(const std::string &cipher, const std::string &key)
{
    if (key.empty() || cipher.empty()) return "";

    const std::string raw = JiYan_B64Url_Decode(cipher);
    if (raw.size() < 24) return "";

    const std::string nonce = raw.substr(0, 16);
    const std::string tag = raw.substr(raw.size() - 8, 8);
    const std::string ct = raw.substr(16, raw.size() - 24);

    const std::string calc = JiYan_HMAC_SHA256_Raw(nonce + ct, key).substr(0, 8);
    if (!JiYan_Secure_Equal(calc, tag)) return "";

    const std::string ks = JiYan_Seal_Keystream(ct.size(), key, nonce);

    std::string out;
    out.reserve(ct.size());
    for (size_t i = 0; i < ct.size(); ++i)
    {
        out.push_back(static_cast<char>(
            static_cast<unsigned char>(ct[i]) ^ static_cast<unsigned char>(ks[i])));
    }
    return out;
}

#endif