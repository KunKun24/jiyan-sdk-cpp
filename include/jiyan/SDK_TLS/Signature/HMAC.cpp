/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_HMAC_CPP
#define JIYAN_HMAC_CPP

#include <string>
#include <cctype>

std::string JiYan_Hex_To_Raw(const std::string &hex)
{
    static const char HEXS[] = "0123456789abcdef";
    int map[256];
    for (int i = 0; i < 256; ++i) map[i] = -1;
    for (int i = 0; i < 16; ++i)
    {
        map[(unsigned char)HEXS[i]] = i;
        map[(unsigned char)toupper((unsigned char)HEXS[i])] = i;
    }

    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        const int hi = map[(unsigned char)hex[i]];
        const int lo = map[(unsigned char)hex[i + 1]];
        if (hi < 0 || lo < 0) return std::string();
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

std::string JiYan_Raw_To_Hex(const std::string &raw)
{
    static const char HEXS[] = "0123456789abcdef";
    std::string out;
    out.reserve(raw.size() * 2);
    for (size_t i = 0; i < raw.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        out.push_back(HEXS[(c >> 4) & 0x0F]);
        out.push_back(HEXS[c & 0x0F]);
    }
    return out;
}

std::string JiYan_HMAC_SHA256_Raw(const std::string &msg, const std::string &key)
{
    const size_t BLOCK = 64;

    std::string k = key;
    if (k.size() > BLOCK) k = JiYan_Hex_To_Raw(JiYan_Signature_SHA256(k));
    k.resize(BLOCK, '\0');

    std::string ipad(BLOCK, '\0');
    std::string opad(BLOCK, '\0');
    for (size_t i = 0; i < BLOCK; ++i)
    {
        ipad[i] = static_cast<char>(static_cast<unsigned char>(k[i]) ^ 0x36);
        opad[i] = static_cast<char>(static_cast<unsigned char>(k[i]) ^ 0x5C);
    }

    const std::string inner = JiYan_Hex_To_Raw(JiYan_Signature_SHA256(ipad + msg));
    return JiYan_Hex_To_Raw(JiYan_Signature_SHA256(opad + inner));
}

std::string JiYan_HMAC_SHA256_Hex(const std::string &msg, const std::string &key)
{
    return JiYan_Raw_To_Hex(JiYan_HMAC_SHA256_Raw(msg, key));
}

std::string JiYan_Signature_HMAC(std::string Content, std::string Key)
{
    return JiYan_HMAC_SHA256_Hex(Content, Key);
}

bool JiYan_Secure_Equal(const std::string &a, const std::string &b)
{
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        diff = static_cast<unsigned char>(
            diff | (static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i])));
    }
    return diff == 0;
}

#endif