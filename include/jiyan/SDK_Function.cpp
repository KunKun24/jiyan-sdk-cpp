/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_FUNCTION_CPP
#define JIYAN_SDK_FUNCTION_CPP

#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <libgen.h>
#include <inttypes.h>

namespace JiYanSDK_Function
{
    bool JiYan_strempty(const std::string &str)
    {
        return str.empty();
    }

    bool JiYan_strcmp(const std::string &str1, const std::string &str2)
    {
        return str1 == str2;
    }

    bool JiYan_strstrstr(const std::string &str1, const std::string &str2)
    {
        return str1.find(str2) != std::string::npos;
    }

    std::string Get_Timestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        return std::to_string(seconds);
    }

    std::string Get_Timestamp_Ms()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return std::to_string(milliseconds);
    }

    std::string Random_String(size_t len)
    {
        static const char CHARS[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, 63);
        std::string out;
        out.reserve(len);
        for (size_t i = 0; i < len; ++i)
        {
            out.push_back(CHARS[dis(gen)]);
        }
        return out;
    }

    std::string Random_Card(const std::string &Prefix, int Days)
    {
        const std::string charset = "ABCDEF0123456789";

        unsigned seed = static_cast<unsigned>(
            std::chrono::system_clock::now().time_since_epoch().count());

        std::mt19937 generator(seed);
        std::uniform_int_distribution<int> distribution(0, static_cast<int>(charset.size()) - 1);

        std::string result = "JiYan-" + Prefix + "-" + std::to_string(Days) + "D-";

        for (int i = 0; i < 8; ++i)
        {
            result += charset[distribution(generator)];
        }
        return result;
    }

    std::string Get_Current_So_Name()
    {
        uintptr_t Address = (uintptr_t)__builtin_return_address(0);
        Dl_info info;
        if (dladdr((void *)Address, &info) && info.dli_fname)
        {
            std::string path(info.dli_fname);
            size_t pos = path.find_last_of('/');
            if (pos != std::string::npos)
            {
                return path.substr(pos + 1);
            }
            return path;
        }
        return std::string();
    }

    std::string Get_File_MD5(const std::string &path)
    {
        if (path.empty()) return "";
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";

        JiYan_MD5_CTX context;
        JiYan_MD5Init(&context);
        char buffer[4096]{};
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
        {
            JiYan_MD5Update(&context, reinterpret_cast<unsigned char *>(buffer),
                            static_cast<unsigned int>(file.gcount()));
        }
        // EOF 是正常结束；目录、介质错误或中途读取失败不能生成部分文件摘要。
        if (file.bad() || !file.eof()) return "";

        unsigned char digest[16]{};
        JiYan_MD5Final(&context, digest);
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (unsigned char byte : digest) ss << std::setw(2) << static_cast<int>(byte);
        return ss.str();
    }

    std::string Get_So_MD5(const std::string &So_Name)
    {
        if (So_Name.empty() || So_Name.find('/') != std::string::npos) return "";
        std::ifstream maps("/proc/self/maps");
        if (!maps.is_open()) return "";
        std::string line;
        while (std::getline(maps, line))
        {
            // maps 前五列固定，路径可能含空格；匿名映射没有路径，必须跳过。
            std::istringstream row(line);
            std::string address, permissions, offset, device, inode, path;
            if (!(row >> address >> permissions >> offset >> device >> inode)) continue;
            std::getline(row, path);
            const auto begin = path.find_first_not_of(" \t");
            if (begin == std::string::npos || path[begin] != '/') continue;
            path.erase(0, begin);
            const auto slash = path.find_last_of('/');
            if (path.substr(slash + 1) == So_Name) return Get_File_MD5(path);
        }
        return "";
    }

    std::string Url_Encode(const std::string &in)
    {
        static const char HEX[] = "0123456789ABCDEF";
        std::string out;
        out.reserve(in.size() * 3);
        for (size_t i = 0; i < in.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(in[i]);
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            {
                out.push_back(static_cast<char>(c));
            }
            else
            {
                out.push_back('%');
                out.push_back(HEX[(c >> 4) & 0x0F]);
                out.push_back(HEX[c & 0x0F]);
            }
        }
        return out;
    }

    std::string Hmac_Sha256_Hex(const std::string &msg, const std::string &key)
    {
        return JiYan_HMAC_SHA256_Hex(msg, key);
    }

    std::string Hex_Encode(const std::string &raw)
    {
        static const char HEX[] = "0123456789abcdef";
        std::string out;
        out.reserve(raw.size() * 2);
        for (size_t i = 0; i < raw.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(raw[i]);
            out.push_back(HEX[(c >> 4) & 0x0F]);
            out.push_back(HEX[c & 0x0F]);
        }
        return out;
    }

    std::string B64_Encode(const std::string &raw)
    {
        static const char CHARS[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        const size_t len = raw.size();
        out.reserve(((len + 2) / 3) * 4);
        size_t i = 0;
        while (i < len)
        {
            const unsigned char a = i < len ? static_cast<unsigned char>(raw[i++]) : 0;
            const unsigned char b = i < len ? static_cast<unsigned char>(raw[i++]) : 0;
            const unsigned char c = i < len ? static_cast<unsigned char>(raw[i++]) : 0;
            const unsigned int triple = (static_cast<unsigned int>(a) << 16) |
                                        (static_cast<unsigned int>(b) << 8) |
                                        static_cast<unsigned int>(c);
            out.push_back(CHARS[(triple >> 18) & 0x3F]);
            out.push_back(CHARS[(triple >> 12) & 0x3F]);
            out.push_back(CHARS[(triple >> 6) & 0x3F]);
            out.push_back(CHARS[triple & 0x3F]);
        }
        if (len % 3 == 1)
        {
            out[out.size() - 2] = '=';
            out[out.size() - 1] = '=';
        }
        else if (len % 3 == 2)
        {
            out[out.size() - 1] = '=';
        }
        return out;
    }

    std::string B64_Decode(const std::string &in)
    {
        static const std::string CHARS =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string clean;
        clean.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            const char c = in[i];
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') clean.push_back(c);
        }

        size_t len = clean.size();
        while (len > 0 && clean[len - 1] == '=') --len;

        std::string out;
        out.reserve((len * 3) / 4 + 3);
        size_t i = 0;
        while (i < len)
        {
            int a = i < len ? static_cast<int>(CHARS.find(clean[i++])) : 0;
            int b = i < len ? static_cast<int>(CHARS.find(clean[i++])) : 0;
            int c = i < len ? static_cast<int>(CHARS.find(clean[i++])) : 0;
            int d = i < len ? static_cast<int>(CHARS.find(clean[i++])) : 0;
            if (a < 0 || b < 0 || c < 0 || d < 0) return std::string();
            const unsigned int triple = (static_cast<unsigned int>(a) << 18) |
                                        (static_cast<unsigned int>(b) << 12) |
                                        (static_cast<unsigned int>(c) << 6) |
                                        static_cast<unsigned int>(d);
            out.push_back(static_cast<char>((triple >> 16) & 0xFF));
            out.push_back(static_cast<char>((triple >> 8) & 0xFF));
            out.push_back(static_cast<char>(triple & 0xFF));
        }

        const size_t padding = clean.size() - len;
        if (padding > 0 && out.size() >= padding)
        {
            out.resize(out.size() - padding);
        }
        return out;
    }

    std::string B64Url_Encode(const std::string &raw)
    {
        std::string s = B64_Encode(raw);
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '+') s[i] = '-';
            else if (s[i] == '/') s[i] = '_';
        }
        while (!s.empty() && s.back() == '=') s.pop_back();
        return s;
    }

    std::string B64Url_Decode(const std::string &in)
    {
        std::string s = in;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '-') s[i] = '+';
            else if (s[i] == '_') s[i] = '/';
        }
        while (s.size() % 4 != 0) s.push_back('=');
        return B64_Decode(s);
    }

    bool Secure_Equal(const std::string &a, const std::string &b)
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
}

#endif