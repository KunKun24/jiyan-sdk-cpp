/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#define SHA1_BLOCK_SIZE     64
#define SHA1_DIGEST_SIZE    20
#define SHA1_ROL32(a, n) (((a) << (n)) | ((a) >> (32 - (n))))

#define SHA1_F1(x, y, z) ((x & y) | (~x & z))
#define SHA1_F2(x, y, z) (x ^ y ^ z)
#define SHA1_F3(x, y, z) ((x & y) | (x & z) | (y & z))
#define SHA1_F4(x, y, z) (x ^ y ^ z)

#define SHA1_K1 0x5A827999
#define SHA1_K2 0x6ED9EBA1
#define SHA1_K3 0x8F1BBCDC
#define SHA1_K4 0xCA62C1D6

typedef struct {
    uint32_t h[5];
    uint32_t w[80];
    uint8_t buffer[64];
    size_t size;
    uint64_t totalSize;
} SHA1_Context;

static const uint8_t SHA1_padding[64] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

void SHA1_Init(SHA1_Context *context) {
    context->h[0] = 0x67452301;
    context->h[1] = 0xEFCDAB89;
    context->h[2] = 0x98BADCFE;
    context->h[3] = 0x10325476;
    context->h[4] = 0xC3D2E1F0;
    context->size = 0;
    context->totalSize = 0;
}

void SHA1_ProcessBlock(SHA1_Context *context) {
    int i;
    uint32_t a, b, c, d, e, temp;
    
    for (i = 0; i < 16; i++) {
        context->w[i] = (uint32_t)(context->buffer[i * 4]) << 24 |
                        (uint32_t)(context->buffer[i * 4 + 1]) << 16 |
                        (uint32_t)(context->buffer[i * 4 + 2]) << 8 |
                        (uint32_t)(context->buffer[i * 4 + 3]);
    }
    
    for (i = 16; i < 80; i++) {
        context->w[i] = SHA1_ROL32(context->w[i - 3] ^ context->w[i - 8] ^ 
                                   context->w[i - 14] ^ context->w[i - 16], 1);
    }
    
    a = context->h[0];
    b = context->h[1];
    c = context->h[2];
    d = context->h[3];
    e = context->h[4];
    
    for (i = 0; i < 20; i++) {
        temp = SHA1_ROL32(a, 5) + SHA1_F1(b, c, d) + e + context->w[i] + SHA1_K1;
        e = d;
        d = c;
        c = SHA1_ROL32(b, 30);
        b = a;
        a = temp;
    }
    
    for (i = 20; i < 40; i++) {
        temp = SHA1_ROL32(a, 5) + SHA1_F2(b, c, d) + e + context->w[i] + SHA1_K2;
        e = d;
        d = c;
        c = SHA1_ROL32(b, 30);
        b = a;
        a = temp;
    }
    
    for (i = 40; i < 60; i++) {
        temp = SHA1_ROL32(a, 5) + SHA1_F3(b, c, d) + e + context->w[i] + SHA1_K3;
        e = d;
        d = c;
        c = SHA1_ROL32(b, 30);
        b = a;
        a = temp;
    }
    
    for (i = 60; i < 80; i++) {
        temp = SHA1_ROL32(a, 5) + SHA1_F4(b, c, d) + e + context->w[i] + SHA1_K4;
        e = d;
        d = c;
        c = SHA1_ROL32(b, 30);
        b = a;
        a = temp;
    }
    
    context->h[0] += a;
    context->h[1] += b;
    context->h[2] += c;
    context->h[3] += d;
    context->h[4] += e;
}

void SHA1_Update(SHA1_Context *context, const void *data, size_t length) {
    size_t n;
    const uint8_t *bytes = (const uint8_t *)data;
    
    while (length > 0) {
        n = (length < (64 - context->size)) ? length : (64 - context->size);
        memcpy(context->buffer + context->size, bytes, n);
        
        context->size += n;
        context->totalSize += n;
        bytes += n;
        length -= n;
        
        if (context->size == 64) {
            SHA1_ProcessBlock(context);
            context->size = 0;
        }
    }
}

void SHA1_Final(SHA1_Context *context, uint8_t *digest) {
    size_t i;
    size_t paddingSize;
    uint64_t totalSize = context->totalSize * 8;
    
    if (context->size < 56) {
        paddingSize = 56 - context->size;
    } else {
        paddingSize = 64 + 56 - context->size;
    }
    
    SHA1_Update(context, SHA1_padding, paddingSize);
    
    for (i = 0; i < 8; i++) {
        context->buffer[56 + i] = (uint8_t)((totalSize >> (56 - i * 8)) & 0xFF);
    }
    
    SHA1_ProcessBlock(context);
    
    for (i = 0; i < (SHA1_DIGEST_SIZE / 4); i++) {
        digest[i * 4] = (uint8_t)((context->h[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((context->h[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((context->h[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = (uint8_t)(context->h[i] & 0xFF);
    }
}

std::string JiYan_Signature_SHA1(std::string input) {
    uint8_t digest[SHA1_DIGEST_SIZE];
    SHA1_Context context;
    
    SHA1_Init(&context);
    SHA1_Update(&context, input.c_str(), input.length());
    SHA1_Final(&context, digest);
    
    std::stringstream ss;
    for (int i = 0; i < SHA1_DIGEST_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}