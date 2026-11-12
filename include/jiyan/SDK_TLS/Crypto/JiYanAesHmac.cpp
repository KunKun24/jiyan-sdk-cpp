/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_AESHMAC_CPP
#define JIYAN_AESHMAC_CPP

#include <string>
#include <vector>
#include <random>
#include <cstdio>

std::string JiYan_HMAC_SHA256_Raw(const std::string &msg, const std::string &key);
bool JiYan_Secure_Equal(const std::string &a, const std::string &b);

static std::string JiYan_AesHmac_Random16()
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

static std::string JiYan_AesHmac_B64Url_Encode(const std::string &raw)
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

static std::string JiYan_AesHmac_B64Url_Decode(const std::string &in)
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

nlohmann::json JiYan_Crypto_AesHmac_Encryption(const std::string &plain, const std::string &key)
{
    nlohmann::json result;
    result["密文"] = "";
    result["IV"] = "";

    if (key.empty()) return result;

    const std::string encKey = JiYan_HMAC_SHA256_Raw("jy1-enc", key);
    const std::string macKey = JiYan_HMAC_SHA256_Raw("jy1-mac", key);
    const std::string iv = JiYan_AesHmac_Random16();

    std::vector<unsigned char> keyVec(encKey.begin(), encKey.end());
    std::vector<unsigned char> ivVec(iv.begin(), iv.end());
    std::vector<unsigned char> plainVec(plain.begin(), plain.end());

    const std::vector<unsigned char> ctVec = AES256_cbc_encrypt(keyVec, ivVec, plainVec);
    const std::string ct(ctVec.begin(), ctVec.end());
    if (ct.empty()) return result;

    const std::string tag = JiYan_HMAC_SHA256_Raw(iv + ct, macKey).substr(0, 16);

    result["密文"] = JiYan_AesHmac_B64Url_Encode(iv + ct + tag);
    return result;
}

std::string JiYan_Crypto_AesHmac_Decryption(const std::string &cipher, const std::string &key)
{
    if (key.empty() || cipher.empty()) return "";

    const std::string raw = JiYan_AesHmac_B64Url_Decode(cipher);
    if (raw.size() < 32) return "";

    const std::string iv = raw.substr(0, 16);
    const std::string tag = raw.substr(raw.size() - 16, 16);
    const std::string ct = raw.substr(16, raw.size() - 32);

    const std::string macKey = JiYan_HMAC_SHA256_Raw("jy1-mac", key);
    const std::string calc = JiYan_HMAC_SHA256_Raw(iv + ct, macKey).substr(0, 16);
    if (!JiYan_Secure_Equal(calc, tag)) return "";

    const std::string encKey = JiYan_HMAC_SHA256_Raw("jy1-enc", key);
    std::vector<unsigned char> keyVec(encKey.begin(), encKey.end());
    std::vector<unsigned char> ivVec(iv.begin(), iv.end());
    std::vector<unsigned char> ctVec(ct.begin(), ct.end());

    const std::vector<unsigned char> plainVec = AES256_cbc_decrypt(keyVec, ivVec, ctVec);
    return std::string(plainVec.begin(), plainVec.end());
}

#endif