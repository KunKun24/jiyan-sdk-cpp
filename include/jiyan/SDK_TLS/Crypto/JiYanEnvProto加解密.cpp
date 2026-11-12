/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>

nlohmann::json JiYan_Crypto_EnvProto_Encryption(const std::string& input_text,const std::string& key) {
    nlohmann::json result;
    int key_hash = 0;
    for (size_t i = 0; i < key.length(); i++) {
        int key_code = static_cast<unsigned char>(key[i]);
        key_hash = (key_hash + key_code * (static_cast<int>(i) + 1)) % 100;
    }
    
    std::stringstream encrypted_hex;
    for (size_t i = 0; i < input_text.length(); i++) {
        unsigned char c = static_cast<unsigned char>(input_text[i]);
        unsigned char key_char = static_cast<unsigned char>(
            key[(i + key_hash) % key.length()]);
        unsigned int encrypted_code = (c ^ key_char) + key_hash + i;
        encrypted_code %= 256;
        encrypted_hex << std::hex << std::setw(2) << std::setfill('0') 
                     << static_cast<unsigned int>(encrypted_code);
    }
    result["密文"] = encrypted_hex.str();
    result["IV"] = "";
    return result;
}


std::string JiYan_Crypto_EnvProto_Decryption(const std::string& encrypted_hex,const std::string& key) {
    int key_hash = 0;
    for (size_t i = 0; i < key.length(); i++) {
        int key_code = static_cast<int>(key[i]);
        key_hash = (key_hash + key_code * (static_cast<int>(i) + 1)) % 100;
    }
    
    std::string decrypted_text;
    for (size_t i = 0; i < encrypted_hex.length(); i += 2) {
        std::string hex_byte = encrypted_hex.substr(i, 2);
        int encrypted_code;
        std::stringstream ss;
        ss << std::hex << hex_byte;
        ss >> encrypted_code;
        size_t char_index = i / 2;
        size_t key_index = (char_index + key_hash) % key.length();
        char key_char = key[key_index];
        int key_code = static_cast<int>(key_char);
        int original_code = encrypted_code - key_hash - static_cast<int>(char_index);
        while (original_code < 0) {
            original_code += 256;
        }
        original_code = original_code % 256;
        original_code = original_code ^ key_code;
        
        decrypted_text += static_cast<char>(original_code);
    }
    return decrypted_text;
}