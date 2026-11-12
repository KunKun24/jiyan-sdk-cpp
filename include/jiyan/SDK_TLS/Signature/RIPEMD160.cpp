/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#define RIPEMD160_BLOCK_SIZE     64
#define RIPEMD160_DIGEST_SIZE    20
#define RIPEMD160_ROL32(a, n) (((a) << (n)) | ((a) >> (32 - (n))))

#define RIPEMD160_F(x, y, z) ((x) ^ (y) ^ (z))
#define RIPEMD160_G(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define RIPEMD160_H(x, y, z) (((x) | ~(y)) ^ (z))
#define RIPEMD160_I(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define RIPEMD160_J(x, y, z) ((x) ^ ((y) | ~(z)))

#define RIPEMD160_FF(a, b, c, d, e, x, s) a += RIPEMD160_F(b, c, d) + (x), a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_GG(a, b, c, d, e, x, s) a += RIPEMD160_G(b, c, d) + (x) + 0x5A827999, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_HH(a, b, c, d, e, x, s) a += RIPEMD160_H(b, c, d) + (x) + 0x6ED9EBA1, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_II(a, b, c, d, e, x, s) a += RIPEMD160_I(b, c, d) + (x) + 0x8F1BBCDC, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_JJ(a, b, c, d, e, x, s) a += RIPEMD160_J(b, c, d) + (x) + 0xA953FD4E, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)

#define RIPEMD160_FFF(a, b, c, d, e, x, s) a += RIPEMD160_F(b, c, d) + (x), a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_GGG(a, b, c, d, e, x, s) a += RIPEMD160_G(b, c, d) + (x) + 0x7A6D76E9, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_HHH(a, b, c, d, e, x, s) a += RIPEMD160_H(b, c, d) + (x) + 0x6D703EF3, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_III(a, b, c, d, e, x, s) a += RIPEMD160_I(b, c, d) + (x) + 0x5C4DD124, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)
#define RIPEMD160_JJJ(a, b, c, d, e, x, s) a += RIPEMD160_J(b, c, d) + (x) + 0x50A28BE6, a = RIPEMD160_ROL32(a, s) + (e), c = RIPEMD160_ROL32(c, 10)

typedef struct {
    uint32_t h[5];
    uint32_t x[16];
    uint8_t buffer[64];
    size_t size;
    uint64_t totalSize;
} RIPEMD160_Context;

static const uint8_t RIPEMD160_padding[64] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

void RIPEMD160_Init(RIPEMD160_Context *context) {
    context->h[0] = 0x67452301;
    context->h[1] = 0xEFCDAB89;
    context->h[2] = 0x98BADCFE;
    context->h[3] = 0x10325476;
    context->h[4] = 0xC3D2E1F0;
    context->size = 0;
    context->totalSize = 0;
}

void RIPEMD160_ProcessBlock(RIPEMD160_Context *context) {
    int i;
    uint32_t aa, bb, cc, dd, ee;
    uint32_t aaa, bbb, ccc, ddd, eee;
    
    aa = context->h[0];
    bb = context->h[1];
    cc = context->h[2];
    dd = context->h[3];
    ee = context->h[4];
    aaa = context->h[0];
    bbb = context->h[1];
    ccc = context->h[2];
    ddd = context->h[3];
    eee = context->h[4];
    
    uint32_t *x = context->x;
    
    for (i = 0; i < 16; i++) {
        x[i] = (uint32_t)(context->buffer[i * 4]) |
               (uint32_t)(context->buffer[i * 4 + 1] << 8) |
               (uint32_t)(context->buffer[i * 4 + 2] << 16) |
               (uint32_t)(context->buffer[i * 4 + 3] << 24);
    }
    
    RIPEMD160_FF(aa, bb, cc, dd, ee, x[0],  11);
    RIPEMD160_FF(ee, aa, bb, cc, dd, x[1],  14);
    RIPEMD160_FF(dd, ee, aa, bb, cc, x[2],  15);
    RIPEMD160_FF(cc, dd, ee, aa, bb, x[3],  12);
    RIPEMD160_FF(bb, cc, dd, ee, aa, x[4],  5);
    RIPEMD160_FF(aa, bb, cc, dd, ee, x[5],  8);
    RIPEMD160_FF(ee, aa, bb, cc, dd, x[6],  7);
    RIPEMD160_FF(dd, ee, aa, bb, cc, x[7],  9);
    RIPEMD160_FF(cc, dd, ee, aa, bb, x[8],  11);
    RIPEMD160_FF(bb, cc, dd, ee, aa, x[9],  13);
    RIPEMD160_FF(aa, bb, cc, dd, ee, x[10], 14);
    RIPEMD160_FF(ee, aa, bb, cc, dd, x[11], 15);
    RIPEMD160_FF(dd, ee, aa, bb, cc, x[12], 6);
    RIPEMD160_FF(cc, dd, ee, aa, bb, x[13], 7);
    RIPEMD160_FF(bb, cc, dd, ee, aa, x[14], 9);
    RIPEMD160_FF(aa, bb, cc, dd, ee, x[15], 8);
    
    RIPEMD160_GG(ee, aa, bb, cc, dd, x[7],  7);
    RIPEMD160_GG(dd, ee, aa, bb, cc, x[4],  6);
    RIPEMD160_GG(cc, dd, ee, aa, bb, x[13], 8);
    RIPEMD160_GG(bb, cc, dd, ee, aa, x[1],  13);
    RIPEMD160_GG(aa, bb, cc, dd, ee, x[10], 11);
    RIPEMD160_GG(ee, aa, bb, cc, dd, x[6],  9);
    RIPEMD160_GG(dd, ee, aa, bb, cc, x[15], 7);
    RIPEMD160_GG(cc, dd, ee, aa, bb, x[3],  15);
    RIPEMD160_GG(bb, cc, dd, ee, aa, x[12], 7);
    RIPEMD160_GG(aa, bb, cc, dd, ee, x[0],  12);
    RIPEMD160_GG(ee, aa, bb, cc, dd, x[9],  15);
    RIPEMD160_GG(dd, ee, aa, bb, cc, x[5],  9);
    RIPEMD160_GG(cc, dd, ee, aa, bb, x[2],  11);
    RIPEMD160_GG(bb, cc, dd, ee, aa, x[14], 7);
    RIPEMD160_GG(aa, bb, cc, dd, ee, x[11], 13);
    RIPEMD160_GG(ee, aa, bb, cc, dd, x[8],  12);
    
    RIPEMD160_HH(dd, ee, aa, bb, cc, x[3],  11);
    RIPEMD160_HH(cc, dd, ee, aa, bb, x[10], 13);
    RIPEMD160_HH(bb, cc, dd, ee, aa, x[14], 6);
    RIPEMD160_HH(aa, bb, cc, dd, ee, x[4],  7);
    RIPEMD160_HH(ee, aa, bb, cc, dd, x[9],  14);
    RIPEMD160_HH(dd, ee, aa, bb, cc, x[15], 9);
    RIPEMD160_HH(cc, dd, ee, aa, bb, x[8],  13);
    RIPEMD160_HH(bb, cc, dd, ee, aa, x[1],  15);
    RIPEMD160_HH(aa, bb, cc, dd, ee, x[2],  14);
    RIPEMD160_HH(ee, aa, bb, cc, dd, x[7],  8);
    RIPEMD160_HH(dd, ee, aa, bb, cc, x[0],  13);
    RIPEMD160_HH(cc, dd, ee, aa, bb, x[6],  6);
    RIPEMD160_HH(bb, cc, dd, ee, aa, x[13], 5);
    RIPEMD160_HH(aa, bb, cc, dd, ee, x[11], 12);
    RIPEMD160_HH(ee, aa, bb, cc, dd, x[5],  7);
    RIPEMD160_HH(dd, ee, aa, bb, cc, x[12], 5);
    
    RIPEMD160_II(cc, dd, ee, aa, bb, x[1],  11);
    RIPEMD160_II(bb, cc, dd, ee, aa, x[9],  12);
    RIPEMD160_II(aa, bb, cc, dd, ee, x[11], 14);
    RIPEMD160_II(ee, aa, bb, cc, dd, x[10], 15);
    RIPEMD160_II(dd, ee, aa, bb, cc, x[0],  14);
    RIPEMD160_II(cc, dd, ee, aa, bb, x[8],  15);
    RIPEMD160_II(bb, cc, dd, ee, aa, x[12], 9);
    RIPEMD160_II(aa, bb, cc, dd, ee, x[4],  8);
    RIPEMD160_II(ee, aa, bb, cc, dd, x[13], 9);
    RIPEMD160_II(dd, ee, aa, bb, cc, x[3],  14);
    RIPEMD160_II(cc, dd, ee, aa, bb, x[7],  5);
    RIPEMD160_II(bb, cc, dd, ee, aa, x[15], 6);
    RIPEMD160_II(aa, bb, cc, dd, ee, x[14], 8);
    RIPEMD160_II(ee, aa, bb, cc, dd, x[5],  6);
    RIPEMD160_II(dd, ee, aa, bb, cc, x[6],  5);
    RIPEMD160_II(cc, dd, ee, aa, bb, x[2],  12);
    
    RIPEMD160_JJ(bb, cc, dd, ee, aa, x[4],  9);
    RIPEMD160_JJ(aa, bb, cc, dd, ee, x[0],  15);
    RIPEMD160_JJ(ee, aa, bb, cc, dd, x[5],  5);
    RIPEMD160_JJ(dd, ee, aa, bb, cc, x[9],  11);
    RIPEMD160_JJ(cc, dd, ee, aa, bb, x[7],  6);
    RIPEMD160_JJ(bb, cc, dd, ee, aa, x[12], 8);
    RIPEMD160_JJ(aa, bb, cc, dd, ee, x[2],  13);
    RIPEMD160_JJ(ee, aa, bb, cc, dd, x[10], 12);
    RIPEMD160_JJ(dd, ee, aa, bb, cc, x[14], 5);
    RIPEMD160_JJ(cc, dd, ee, aa, bb, x[1],  12);
    RIPEMD160_JJ(bb, cc, dd, ee, aa, x[3],  13);
    RIPEMD160_JJ(aa, bb, cc, dd, ee, x[8],  14);
    RIPEMD160_JJ(ee, aa, bb, cc, dd, x[11], 11);
    RIPEMD160_JJ(dd, ee, aa, bb, cc, x[6],  8);
    RIPEMD160_JJ(cc, dd, ee, aa, bb, x[15], 5);
    RIPEMD160_JJ(bb, cc, dd, ee, aa, x[13], 6);
    
    RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, x[5],  8);
    RIPEMD160_JJJ(eee, aaa, bbb, ccc, ddd, x[14], 9);
    RIPEMD160_JJJ(ddd, eee, aaa, bbb, ccc, x[7],  9);
    RIPEMD160_JJJ(ccc, ddd, eee, aaa, bbb, x[0],  11);
    RIPEMD160_JJJ(bbb, ccc, ddd, eee, aaa, x[9],  13);
    RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, x[2],  15);
    RIPEMD160_JJJ(eee, aaa, bbb, ccc, ddd, x[11], 15);
    RIPEMD160_JJJ(ddd, eee, aaa, bbb, ccc, x[4],  5);
    RIPEMD160_JJJ(ccc, ddd, eee, aaa, bbb, x[13], 7);
    RIPEMD160_JJJ(bbb, ccc, ddd, eee, aaa, x[6],  7);
    RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, x[15], 8);
    RIPEMD160_JJJ(eee, aaa, bbb, ccc, ddd, x[8],  11);
    RIPEMD160_JJJ(ddd, eee, aaa, bbb, ccc, x[1],  14);
    RIPEMD160_JJJ(ccc, ddd, eee, aaa, bbb, x[10], 14);
    RIPEMD160_JJJ(bbb, ccc, ddd, eee, aaa, x[3],  12);
    RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, x[12], 6);
    
    RIPEMD160_III(eee, aaa, bbb, ccc, ddd, x[6],   9);
    RIPEMD160_III(ddd, eee, aaa, bbb, ccc, x[11], 13);
    RIPEMD160_III(ccc, ddd, eee, aaa, bbb, x[3],  15);
    RIPEMD160_III(bbb, ccc, ddd, eee, aaa, x[7],  7);
    RIPEMD160_III(aaa, bbb, ccc, ddd, eee, x[0],  12);
    RIPEMD160_III(eee, aaa, bbb, ccc, ddd, x[13], 8);
    RIPEMD160_III(ddd, eee, aaa, bbb, ccc, x[5],  9);
    RIPEMD160_III(ccc, ddd, eee, aaa, bbb, x[10], 11);
    RIPEMD160_III(bbb, ccc, ddd, eee, aaa, x[14], 7);
    RIPEMD160_III(aaa, bbb, ccc, ddd, eee, x[15], 7);
    RIPEMD160_III(eee, aaa, bbb, ccc, ddd, x[8],  12);
    RIPEMD160_III(ddd, eee, aaa, bbb, ccc, x[12], 7);
    RIPEMD160_III(ccc, ddd, eee, aaa, bbb, x[4],  6);
    RIPEMD160_III(bbb, ccc, ddd, eee, aaa, x[9],  15);
    RIPEMD160_III(aaa, bbb, ccc, ddd, eee, x[1],  13);
    RIPEMD160_III(eee, aaa, bbb, ccc, ddd, x[2],  11);
    
    RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, x[15], 9);
    RIPEMD160_HHH(ccc, ddd, eee, aaa, bbb, x[5],  7);
    RIPEMD160_HHH(bbb, ccc, ddd, eee, aaa, x[1],  15);
    RIPEMD160_HHH(aaa, bbb, ccc, ddd, eee, x[3],  11);
    RIPEMD160_HHH(eee, aaa, bbb, ccc, ddd, x[7],  8);
    RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, x[14], 6);
    RIPEMD160_HHH(ccc, ddd, eee, aaa, bbb, x[6],  6);
    RIPEMD160_HHH(bbb, ccc, ddd, eee, aaa, x[9],  14);
    RIPEMD160_HHH(aaa, bbb, ccc, ddd, eee, x[11], 12);
    RIPEMD160_HHH(eee, aaa, bbb, ccc, ddd, x[8],  13);
    RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, x[12], 5);
    RIPEMD160_HHH(ccc, ddd, eee, aaa, bbb, x[2],  14);
    RIPEMD160_HHH(bbb, ccc, ddd, eee, aaa, x[10], 13);
    RIPEMD160_HHH(aaa, bbb, ccc, ddd, eee, x[0],  13);
    RIPEMD160_HHH(eee, aaa, bbb, ccc, ddd, x[4],  7);
    RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, x[13], 5);
    
    RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, x[8],  15);
    RIPEMD160_GGG(bbb, ccc, ddd, eee, aaa, x[6],  5);
    RIPEMD160_GGG(aaa, bbb, ccc, ddd, eee, x[4],  8);
    RIPEMD160_GGG(eee, aaa, bbb, ccc, ddd, x[1],  11);
    RIPEMD160_GGG(ddd, eee, aaa, bbb, ccc, x[3],  14);
    RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, x[11], 14);
    RIPEMD160_GGG(bbb, ccc, ddd, eee, aaa, x[15], 6);
    RIPEMD160_GGG(aaa, bbb, ccc, ddd, eee, x[0],  14);
    RIPEMD160_GGG(eee, aaa, bbb, ccc, ddd, x[5],  6);
    RIPEMD160_GGG(ddd, eee, aaa, bbb, ccc, x[12], 9);
    RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, x[2],  12);
    RIPEMD160_GGG(bbb, ccc, ddd, eee, aaa, x[13], 9);
    RIPEMD160_GGG(aaa, bbb, ccc, ddd, eee, x[9],  12);
    RIPEMD160_GGG(eee, aaa, bbb, ccc, ddd, x[7],  5);
    RIPEMD160_GGG(ddd, eee, aaa, bbb, ccc, x[10], 15);
    RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, x[14], 8);
    
    RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, x[12], 8);
    RIPEMD160_FFF(aaa, bbb, ccc, ddd, eee, x[15], 5);
    RIPEMD160_FFF(eee, aaa, bbb, ccc, ddd, x[10], 12);
    RIPEMD160_FFF(ddd, eee, aaa, bbb, ccc, x[4],  9);
    RIPEMD160_FFF(ccc, ddd, eee, aaa, bbb, x[1],  12);
    RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, x[5],  5);
    RIPEMD160_FFF(aaa, bbb, ccc, ddd, eee, x[8],  14);
    RIPEMD160_FFF(eee, aaa, bbb, ccc, ddd, x[7],  6);
    RIPEMD160_FFF(ddd, eee, aaa, bbb, ccc, x[6],  8);
    RIPEMD160_FFF(ccc, ddd, eee, aaa, bbb, x[2],  13);
    RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, x[13], 6);
    RIPEMD160_FFF(aaa, bbb, ccc, ddd, eee, x[14], 5);
    RIPEMD160_FFF(eee, aaa, bbb, ccc, ddd, x[0],  15);
    RIPEMD160_FFF(ddd, eee, aaa, bbb, ccc, x[3],  13);
    RIPEMD160_FFF(ccc, ddd, eee, aaa, bbb, x[9],  11);
    RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, x[11], 11);
    
    ddd = context->h[1] + cc + ddd;
    context->h[1] = context->h[2] + dd + eee;
    context->h[2] = context->h[3] + ee + aaa;
    context->h[3] = context->h[4] + aa + bbb;
    context->h[4] = context->h[0] + bb + ccc;
    context->h[0] = ddd;
}

void RIPEMD160_Update(RIPEMD160_Context *context, const void *data, size_t length) {
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
            RIPEMD160_ProcessBlock(context);
            context->size = 0;
        }
    }
}

void RIPEMD160_Final(RIPEMD160_Context *context, uint8_t *digest) {
    size_t i;
    size_t paddingSize;
    uint64_t totalSize = context->totalSize * 8;
    
    if (context->size < 56) {
        paddingSize = 56 - context->size;
    } else {
        paddingSize = 64 + 56 - context->size;
    }
    
    RIPEMD160_Update(context, RIPEMD160_padding, paddingSize);
    
    for (i = 0; i < 8; i++) {
        context->buffer[56 + i] = (uint8_t)(totalSize & 0xFF);
        totalSize >>= 8;
    }
    
    RIPEMD160_ProcessBlock(context);
    
    for (i = 0; i < (RIPEMD160_DIGEST_SIZE / 4); i++) {
        digest[i * 4] = (uint8_t)(context->h[i] & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((context->h[i] >> 8) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((context->h[i] >> 16) & 0xFF);
        digest[i * 4 + 3] = (uint8_t)((context->h[i] >> 24) & 0xFF);
    }
}

std::string JiYan_Signature_RIPEMD160(std::string input) {
    uint8_t digest[RIPEMD160_DIGEST_SIZE];
    RIPEMD160_Context context;
    
    RIPEMD160_Init(&context);
    RIPEMD160_Update(&context, input.c_str(), input.length());
    RIPEMD160_Final(&context, digest);
    
    std::stringstream ss;
    for (int i = 0; i < RIPEMD160_DIGEST_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}