/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SIGNATURE_CPP
#define JIYAN_SIGNATURE_CPP

#include <iomanip>
#include <sstream>
#include "Signature/MD5.cpp"
#include "Signature/SHA512.cpp"
#include "Signature/CRC32.cpp"
#include "Signature/SHA256.cpp"
#include "Signature/RIPEMD160.cpp"
#include "Signature/SHA1.cpp"
#include "Signature/SHA224.cpp"
#include "Signature/SHA384.cpp"
#include "Signature/SHA3512.cpp"
#include "Signature/SHA3256.cpp"
#include "Signature/SHA3224.cpp"
#include "Signature/HMAC.cpp"
#include "Signature/BLAKE2b.cpp"

std::string JiYan_Signature_Content(const std::string& Content, const App_Info& app_info){
    std::string result = "";

    int Signature_Method = app_info.Signature_Method;
    std::string Signature_Key = app_info.Signature_Key;

    std::string Signature_Content = Content + Signature_Key;

    if(Signature_Method == 0)//不签名
    result = "";

    if(Signature_Method == 1)//MD5
    result = JiYan_Signature_MD5(Signature_Content);

    if(Signature_Method == 2)//SHA512
    result = JiYan_Signature_SHA512(Signature_Content);

    if(Signature_Method == 3)//CRC32
    result = JiYan_Signature_CRC32(Signature_Content);

    if(Signature_Method == 4)//SHA256
    result = JiYan_Signature_SHA256(Signature_Content);

    if(Signature_Method == 5)//RIPEMD160
    result = JiYan_Signature_RIPEMD160(Signature_Content);

    if(Signature_Method == 6)//SHA1
    result = JiYan_Signature_SHA1(Signature_Content);

    if(Signature_Method == 7)//SHA224
    result = JiYan_Signature_SHA224(Signature_Content);

    if(Signature_Method == 8)//SHA384
    result = JiYan_Signature_SHA384(Signature_Content);

    if(Signature_Method == 9)//SHA3-512
    result = JiYan_Signature_SHA3512(Signature_Content);

    if(Signature_Method == 10)//SHA3-256
    result = JiYan_Signature_SHA3256(Signature_Content);

    if(Signature_Method == 11)//SHA3-224
    result = JiYan_Signature_SHA3224(Signature_Content);

    if(Signature_Method == 12)//HMAC-SHA256，msg 不含 key
    result = JiYan_HMAC_SHA256_Hex(Content, Signature_Key);

    if(Signature_Method == 13)//HMAC-SHA256 + "." + 前16位
    result = JiYan_HMAC_SHA256_Hex(Content, Signature_Key) + "."
           + JiYan_HMAC_SHA256_Hex("jy1" + Content + Signature_Key, Signature_Key).substr(0, 16);

    if(Signature_Method == 14)//BLAKE2b-256（带密钥，key = SHA-256(sign_key) 原始 32 字节）
    {
        if(Signature_Key.empty())
        {
            result = "";
        }
        else
        {
            const std::string key32 = JiYan_Hex_To_Raw(JiYan_Signature_SHA256(Signature_Key));
            result = JiYan_Blake2b_256_Keyed_Hex(Content, key32);
        }
    }

    return result;
}

std::string JiYan_Signature_Content(std::string Content) {
    return JiYan_Signature_Content(Content, JiYanSDK::GetRuntimeSnapshot().app);
}

#endif