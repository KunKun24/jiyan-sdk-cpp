/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <string>
#include <algorithm>
#include <random>

nlohmann::json JiYan_Crypto_Base64_Encryption(const std::string& input,const std::string& key) {
    nlohmann::json result;
    std::string result2;
    size_t i = 0;
    size_t len = input.size();
    
    while (i < len) {
        unsigned char a = i < len ? input[i++] : 0;
        unsigned char b = i < len ? input[i++] : 0;
        unsigned char c = i < len ? input[i++] : 0;
        
        unsigned int triple = (a << 16) | (b << 8) | c;
        
        result2.push_back(key[(triple >> 18) & 0x3F]);
        result2.push_back(key[(triple >> 12) & 0x3F]);
        result2.push_back(key[(triple >> 6) & 0x3F]);
        result2.push_back(key[triple & 0x3F]);
    }
 
    if (len % 3 == 1) {
        result2[result2.size() - 2] = '=';
        result2[result2.size() - 1] = '=';
    } else if (len % 3 == 2) {
        result2[result2.size() - 1] = '=';
    }
    

        result["密文"] = result2;
        result["IV"] = "";
        return result;
}


std::string JiYan_Crypto_Base64_Decryption(const std::string& input,const std::string& key) {
    std::string result;
    size_t i = 0;
    size_t len = input.size();

    while (len > 0 && input[len - 1] == '=') {
        len--;
    }
    
    auto get_index = [&key](char c) -> int {
        size_t pos = key.find(c);
        return pos != std::string::npos ? (int)pos : -1;
    };
    
    while (i < len) {
        int a = i < len ? get_index(input[i++]) : 0;
        int b = i < len ? get_index(input[i++]) : 0;
        int c = i < len ? get_index(input[i++]) : 0;
        int d = i < len ? get_index(input[i++]) : 0;
        
        if (a < 0 || b < 0 || c < 0 || d < 0) {
            return "";
        }
        
        unsigned int triple = (a << 18) | (b << 12) | (c << 6) | d;
        
        result.push_back((triple >> 16) & 0xFF);
        result.push_back((triple >> 8) & 0xFF);
        result.push_back(triple & 0xFF);
    }

    size_t padding = input.size() - len;
    if (padding > 0) {
        result.resize(result.size() - padding);
    }
    
    return result;
}