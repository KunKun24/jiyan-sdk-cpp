/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <random>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

const char AES256_base64_chars[] = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string AES256_IV = "";

const unsigned char AES256_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
    0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
    0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
    0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
    0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
    0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
    0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
    0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
    0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

const unsigned char AES256_rsbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
    0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
    0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
    0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
    0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
    0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
    0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
    0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
    0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
    0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
    0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
    0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
    0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
    0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
    0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
    0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

const unsigned char AES256_Rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

char* AES256_base64_encode(const unsigned char* data, size_t input_length) {
    size_t output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = (char*)malloc(output_length + 1);
    
    if (encoded_data == NULL) return NULL;
    
    size_t i, j;
    for (i = 0, j = 0; i < input_length;) {
        unsigned int octet_a = i < input_length ? data[i++] : 0;
        unsigned int octet_b = i < input_length ? data[i++] : 0;
        unsigned int octet_c = i < input_length ? data[i++] : 0;
        unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        encoded_data[j++] = AES256_base64_chars[(triple >> 18) & 0x3F];
        encoded_data[j++] = AES256_base64_chars[(triple >> 12) & 0x3F];
        encoded_data[j++] = AES256_base64_chars[(triple >> 6) & 0x3F];
        encoded_data[j++] = AES256_base64_chars[triple & 0x3F];
    }

    if (input_length % 3 == 1) {
        encoded_data[output_length - 1] = '=';
        encoded_data[output_length - 2] = '=';
    } else if (input_length % 3 == 2) {
        encoded_data[output_length - 1] = '=';
    }
    
    encoded_data[output_length] = '\0';
    return encoded_data;
}

unsigned char AES256_base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return 0;
    return 0;
}

std::vector<unsigned char> AES256_base64_decode(const std::string& encoded) {
    std::string clean_encoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        char c = encoded[i];
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            clean_encoded += c;
        }
    }
    
    size_t input_length = clean_encoded.length();
    if (input_length == 0) {
        return std::vector<unsigned char>();
    }
    
    size_t padding = 0;
    if (input_length > 0) {
        if (clean_encoded[input_length - 1] == '=') padding++;
        if (input_length > 1 && clean_encoded[input_length - 2] == '=') padding++;
    }
    
    size_t output_length = (input_length * 3) / 4 - padding;
    std::vector<unsigned char> decoded_data(output_length);
    
    size_t i = 0, j = 0;
    unsigned int sextet_a, sextet_b, sextet_c, sextet_d;
    
    while (i < input_length) {
        sextet_a = (i < input_length) ? AES256_base64_decode_char(clean_encoded[i++]) : 0;
        sextet_b = (i < input_length) ? AES256_base64_decode_char(clean_encoded[i++]) : 0;
        sextet_c = (i < input_length) ? AES256_base64_decode_char(clean_encoded[i++]) : 0;
        sextet_d = (i < input_length) ? AES256_base64_decode_char(clean_encoded[i++]) : 0;

        unsigned int triple = (sextet_a << 18) | (sextet_b << 12) | 
                             (sextet_c << 6) | sextet_d;

        if (j < output_length) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < output_length) decoded_data[j++] = (triple >> 8) & 0xFF;
        if (j < output_length) decoded_data[j++] = triple & 0xFF;
    }
    
    return decoded_data;
}

std::string AES256_base64_encode_string(const std::vector<unsigned char>& data) {
    char* encoded = AES256_base64_encode(data.data(), data.size());
    if (!encoded) return "";
    std::string result(encoded);
    free(encoded);
    return result;
}

void AES256_KeyExpansion(const unsigned char* key, unsigned char* roundKeys) {
    unsigned char temp[4];
    int i = 0;
    
    while (i < 8) {
        for (int j = 0; j < 4; j++) {
            roundKeys[(i * 4) + j] = key[(i * 4) + j];
        }
        i++;
    }
    
    while (i < 60) {
        for (int j = 0; j < 4; j++) {
            temp[j] = roundKeys[(i - 1) * 4 + j];
        }
        
        if (i % 8 == 0) {
            unsigned char k = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = k;
            
            temp[0] = AES256_sbox[temp[0]];
            temp[1] = AES256_sbox[temp[1]];
            temp[2] = AES256_sbox[temp[2]];
            temp[3] = AES256_sbox[temp[3]];
            
            temp[0] ^= AES256_Rcon[i / 8];
        } else if (i % 8 == 4) {
            temp[0] = AES256_sbox[temp[0]];
            temp[1] = AES256_sbox[temp[1]];
            temp[2] = AES256_sbox[temp[2]];
            temp[3] = AES256_sbox[temp[3]];
        }
        
        for (int j = 0; j < 4; j++) {
            roundKeys[i * 4 + j] = roundKeys[(i - 8) * 4 + j] ^ temp[j];
        }
        
        i++;
    }
}

void AES256_SubBytes(unsigned char* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = AES256_sbox[state[i]];
    }
}

void AES256_InvSubBytes(unsigned char* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = AES256_rsbox[state[i]];
    }
}

void AES256_ShiftRows(unsigned char* state) {
    unsigned char temp;
    
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;
    
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    temp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = temp;
}

void AES256_InvShiftRows(unsigned char* state) {
    unsigned char temp;
    
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;
    
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    temp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temp;
}

unsigned char AES256_galois_mul(unsigned char a, unsigned char b) {
    unsigned char p = 0;
    unsigned char hi_bit_set;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        hi_bit_set = (a & 0x80);
        a <<= 1;
        if (hi_bit_set) a ^= 0x1B;
        b >>= 1;
    }
    return p;
}

void AES256_MixColumns(unsigned char* state) {
    unsigned char tmp[16];
    
    for (int i = 0; i < 4; i++) {
        tmp[i * 4 + 0] = (unsigned char)(AES256_galois_mul(0x02, state[i * 4 + 0]) ^ 
                                        AES256_galois_mul(0x03, state[i * 4 + 1]) ^ 
                                        state[i * 4 + 2] ^ 
                                        state[i * 4 + 3]);
        
        tmp[i * 4 + 1] = (unsigned char)(state[i * 4 + 0] ^ 
                                        AES256_galois_mul(0x02, state[i * 4 + 1]) ^ 
                                        AES256_galois_mul(0x03, state[i * 4 + 2]) ^ 
                                        state[i * 4 + 3]);
        
        tmp[i * 4 + 2] = (unsigned char)(state[i * 4 + 0] ^ 
                                        state[i * 4 + 1] ^ 
                                        AES256_galois_mul(0x02, state[i * 4 + 2]) ^ 
                                        AES256_galois_mul(0x03, state[i * 4 + 3]));
        
        tmp[i * 4 + 3] = (unsigned char)(AES256_galois_mul(0x03, state[i * 4 + 0]) ^ 
                                        state[i * 4 + 1] ^ 
                                        state[i * 4 + 2] ^ 
                                        AES256_galois_mul(0x02, state[i * 4 + 3]));
    }
    
    memcpy(state, tmp, 16);
}

void AES256_InvMixColumns(unsigned char* state) {
    unsigned char tmp[16];
    
    for (int i = 0; i < 4; i++) {
        tmp[i * 4 + 0] = (unsigned char)(AES256_galois_mul(0x0e, state[i * 4 + 0]) ^ 
                                        AES256_galois_mul(0x0b, state[i * 4 + 1]) ^ 
                                        AES256_galois_mul(0x0d, state[i * 4 + 2]) ^ 
                                        AES256_galois_mul(0x09, state[i * 4 + 3]));
        
        tmp[i * 4 + 1] = (unsigned char)(AES256_galois_mul(0x09, state[i * 4 + 0]) ^ 
                                        AES256_galois_mul(0x0e, state[i * 4 + 1]) ^ 
                                        AES256_galois_mul(0x0b, state[i * 4 + 2]) ^ 
                                        AES256_galois_mul(0x0d, state[i * 4 + 3]));
        
        tmp[i * 4 + 2] = (unsigned char)(AES256_galois_mul(0x0d, state[i * 4 + 0]) ^ 
                                        AES256_galois_mul(0x09, state[i * 4 + 1]) ^ 
                                        AES256_galois_mul(0x0e, state[i * 4 + 2]) ^ 
                                        AES256_galois_mul(0x0b, state[i * 4 + 3]));
        
        tmp[i * 4 + 3] = (unsigned char)(AES256_galois_mul(0x0b, state[i * 4 + 0]) ^ 
                                        AES256_galois_mul(0x0d, state[i * 4 + 1]) ^ 
                                        AES256_galois_mul(0x09, state[i * 4 + 2]) ^ 
                                        AES256_galois_mul(0x0e, state[i * 4 + 3]));
    }
    
    memcpy(state, tmp, 16);
}

void AES256_AddRoundKey(unsigned char* state, const unsigned char* roundKey) {
    for (int i = 0; i < 16; i++) {
        state[i] ^= roundKey[i];
    }
}

void AES256_EncryptBlock(unsigned char* state, const unsigned char* roundKeys) {
    AES256_AddRoundKey(state, roundKeys);
    
    for (int round = 1; round < 14; round++) {
        AES256_SubBytes(state);
        AES256_ShiftRows(state);
        AES256_MixColumns(state);
        AES256_AddRoundKey(state, roundKeys + round * 16);
    }
    
    AES256_SubBytes(state);
    AES256_ShiftRows(state);
    AES256_AddRoundKey(state, roundKeys + 14 * 16);
}

void AES256_DecryptBlock(unsigned char* state, const unsigned char* roundKeys) {
    AES256_AddRoundKey(state, roundKeys + 14 * 16);
    
    for (int round = 13; round > 0; round--) {
        AES256_InvShiftRows(state);
        AES256_InvSubBytes(state);
        AES256_AddRoundKey(state, roundKeys + round * 16);
        AES256_InvMixColumns(state);
    }
    
    AES256_InvShiftRows(state);
    AES256_InvSubBytes(state);
    AES256_AddRoundKey(state, roundKeys);
}

std::vector<unsigned char> AES256_pkcs7_pad(const std::vector<unsigned char>& data) {
    size_t block_size = 16;
    size_t padding_len = block_size - (data.size() % block_size);
    std::vector<unsigned char> padded = data;
    for (size_t i = 0; i < padding_len; ++i) {
        padded.push_back((unsigned char)padding_len);
    }
    return padded;
}

std::vector<unsigned char> AES256_pkcs7_unpad(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return data;
    }
    
    size_t data_len = data.size();
    unsigned char padding_len = data[data_len - 1];
    
    if (padding_len > 16 || padding_len == 0 || padding_len > data_len) {
        return {};
    }
    
    for (size_t i = data_len - padding_len; i < data_len; ++i) {
        if (data[i] != padding_len) {
            return {};
        }
    }
    
    std::vector<unsigned char> result(data.begin(), data.begin() + data_len - padding_len);
    return result;
}

std::vector<unsigned char> AES256_cbc_encrypt(const std::vector<unsigned char>& key, 
                                          const std::vector<unsigned char>& iv, 
                                          const std::vector<unsigned char>& plaintext) {
    if (key.size() != 32 || iv.size() != 16) return {};
    unsigned char roundKeys[240];
    AES256_KeyExpansion(key.data(), roundKeys);
    
    std::vector<unsigned char> padded = AES256_pkcs7_pad(plaintext);
    std::vector<unsigned char> ciphertext(padded.size());
    unsigned char current_iv[16];
    memcpy(current_iv, iv.data(), 16);
    
    for (size_t i = 0; i < padded.size(); i += 16) {
        unsigned char block[16];
        
        for (int j = 0; j < 16; j++) {
            block[j] = padded[i + j] ^ current_iv[j];
        }
        
        AES256_EncryptBlock(block, roundKeys);
        
        for (int j = 0; j < 16; j++) {
            ciphertext[i + j] = block[j];
            current_iv[j] = block[j];
        }
    }
    
    return ciphertext;
}

std::vector<unsigned char> AES256_cbc_decrypt(const std::vector<unsigned char>& key, 
                                          const std::vector<unsigned char>& iv, 
                                          const std::vector<unsigned char>& ciphertext) {
    if (key.size() != 32 || iv.size() != 16) return {};
    unsigned char roundKeys[240];
    AES256_KeyExpansion(key.data(), roundKeys);
    
    if (ciphertext.empty() || ciphertext.size() % 16 != 0) return {};
    std::vector<unsigned char> plaintext(ciphertext.size());
    unsigned char current_iv[16];
    memcpy(current_iv, iv.data(), 16);
    
    for (size_t i = 0; i < ciphertext.size(); i += 16) {
        unsigned char block[16];
        memcpy(block, &ciphertext[i], 16);
        
        unsigned char saved_cipher[16];
        memcpy(saved_cipher, block, 16);
        
        AES256_DecryptBlock(block, roundKeys);
        
        for (int j = 0; j < 16; j++) {
            plaintext[i + j] = block[j] ^ current_iv[j];
        }
        
        memcpy(current_iv, saved_cipher, 16);
    }
    
    return AES256_pkcs7_unpad(plaintext);
}

std::string AES256_GenerateIV() {
    std::vector<unsigned char> iv(16);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 16; i++) {
        iv[i] = static_cast<unsigned char>(dis(gen));
    }
    std::string iv_base64 = AES256_base64_encode_string(iv);
    return iv_base64;
}

nlohmann::json JiYan_Crypto_AES256CBC_Encryption(const std::string& plaintext, const std::string& key, const std::string& iv) {
    auto aes_key = AES256_base64_decode(key);
    aes_key.resize(32, 0); // 与 PHP fitAesKey 一致，截断或补零至32字节。
    const auto iv_bytes = AES256_base64_decode(iv);
    const auto cipher = AES256_cbc_encrypt(aes_key, iv_bytes,
        std::vector<unsigned char>(plaintext.begin(), plaintext.end()));
    return {{"密文", AES256_base64_encode_string(cipher)}, {"IV", iv}};
}

nlohmann::json JiYan_Crypto_AES256CBC_Encryption(const std::string& plaintext, const std::string& key) {
    AES256_IV = AES256_GenerateIV();
    return JiYan_Crypto_AES256CBC_Encryption(plaintext, key, AES256_IV);
}

std::string JiYan_Crypto_AES256CBC_Decryption(const std::string& ciphertext, const std::string& key, const std::string& iv) {
    auto aes_key = AES256_base64_decode(key);
    aes_key.resize(32, 0);
    const auto plain = AES256_cbc_decrypt(aes_key, AES256_base64_decode(iv), AES256_base64_decode(ciphertext));
    return std::string(plain.begin(), plain.end());
}

std::string JiYan_Crypto_AES256CBC_Decryption(const std::string& ciphertext, const std::string& key) {
    const auto iv = AES256_IV;
    AES256_IV.clear();
    return JiYan_Crypto_AES256CBC_Decryption(ciphertext, key, iv);
}
