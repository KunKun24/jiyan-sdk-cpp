/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#define SHA224_BLOCK_SIZE     64
#define SHA224_DIGEST_SIZE    28
#define SHA224_ROL32(a, n) (((a) << (n)) | ((a) >> (32 - (n))))
#define SHA224_ROR32(a, n) (((a) >> (n)) | ((a) << (32 - (n))))

#define SHA224_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA224_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA224_BSIG0(x) (SHA224_ROR32((x), 2) ^ SHA224_ROR32((x), 13) ^ SHA224_ROR32((x), 22))
#define SHA224_BSIG1(x) (SHA224_ROR32((x), 6) ^ SHA224_ROR32((x), 11) ^ SHA224_ROR32((x), 25))
#define SHA224_SSIG0(x) (SHA224_ROR32((x), 7) ^ SHA224_ROR32((x), 18) ^ ((x) >> 3))
#define SHA224_SSIG1(x) (SHA224_ROR32((x), 17) ^ SHA224_ROR32((x), 19) ^ ((x) >> 10))

static const uint32_t SHA224_H0[8] = {
    0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
    0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4
};

static const uint32_t SHA224_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

typedef struct {
    uint32_t h[8];
    uint32_t w[64];
    uint8_t buffer[64];
    size_t size;
    uint64_t totalSize;
} SHA224_Context;

static const uint8_t SHA224_padding[64] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

void SHA224_Init(SHA224_Context *context) {
    memcpy(context->h, SHA224_H0, sizeof(SHA224_H0));
    context->size = 0;
    context->totalSize = 0;
}

void SHA224_ProcessBlock(SHA224_Context *context) {
    int i;
    uint32_t a, b, c, d, e, f, g, h, t1, t2;
    
    for (i = 0; i < 16; i++) {
        context->w[i] = (uint32_t)(context->buffer[i * 4]) << 24 |
                        (uint32_t)(context->buffer[i * 4 + 1]) << 16 |
                        (uint32_t)(context->buffer[i * 4 + 2]) << 8 |
                        (uint32_t)(context->buffer[i * 4 + 3]);
    }
    
    for (i = 16; i < 64; i++) {
        context->w[i] = SHA224_SSIG1(context->w[i - 2]) + context->w[i - 7] + 
                        SHA224_SSIG0(context->w[i - 15]) + context->w[i - 16];
    }
    
    a = context->h[0];
    b = context->h[1];
    c = context->h[2];
    d = context->h[3];
    e = context->h[4];
    f = context->h[5];
    g = context->h[6];
    h = context->h[7];
    
    for (i = 0; i < 64; i++) {
        t1 = h + SHA224_BSIG1(e) + SHA224_CH(e, f, g) + SHA224_K[i] + context->w[i];
        t2 = SHA224_BSIG0(a) + SHA224_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    context->h[0] += a;
    context->h[1] += b;
    context->h[2] += c;
    context->h[3] += d;
    context->h[4] += e;
    context->h[5] += f;
    context->h[6] += g;
    context->h[7] += h;
}

void SHA224_Update(SHA224_Context *context, const void *data, size_t length) {
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
            SHA224_ProcessBlock(context);
            context->size = 0;
        }
    }
}

void SHA224_Final(SHA224_Context *context, uint8_t *digest) {
    size_t i;
    size_t paddingSize;
    uint64_t totalSize = context->totalSize * 8;
    
    if (context->size < 56) {
        paddingSize = 56 - context->size;
    } else {
        paddingSize = 64 + 56 - context->size;
    }
    
    SHA224_Update(context, SHA224_padding, paddingSize);
    
    for (i = 0; i < 8; i++) {
        context->buffer[56 + i] = (uint8_t)((totalSize >> (56 - i * 8)) & 0xFF);
    }
    
    SHA224_ProcessBlock(context);
    
    for (i = 0; i < (SHA224_DIGEST_SIZE / 4); i++) {
        digest[i * 4] = (uint8_t)((context->h[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((context->h[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((context->h[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = (uint8_t)(context->h[i] & 0xFF);
    }
}

std::string JiYan_Signature_SHA224(std::string input) {
    uint8_t digest[SHA224_DIGEST_SIZE];
    SHA224_Context context;
    
    SHA224_Init(&context);
    SHA224_Update(&context, input.c_str(), input.length());
    SHA224_Final(&context, digest);
    
    std::stringstream ss;
    for (int i = 0; i < SHA224_DIGEST_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}