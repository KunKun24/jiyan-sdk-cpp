/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#define SHA384_BLOCK_SIZE     128
#define SHA384_DIGEST_SIZE    48

typedef struct {
    uint64_t state[8];
    uint64_t bitcount;
    uint8_t buffer[128];
    size_t buffer_len;
} SHA384_Context;

static const uint64_t SHA384_K[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
    0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

#define SHA384_ROTR(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

#define SHA384_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA384_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA384_EP0(x) (SHA384_ROTR(x, 28) ^ SHA384_ROTR(x, 34) ^ SHA384_ROTR(x, 39))
#define SHA384_EP1(x) (SHA384_ROTR(x, 14) ^ SHA384_ROTR(x, 18) ^ SHA384_ROTR(x, 41))
#define SHA384_SN0(x) (SHA384_ROTR(x, 1) ^ SHA384_ROTR(x, 8) ^ ((x) >> 7))
#define SHA384_SN1(x) (SHA384_ROTR(x, 19) ^ SHA384_ROTR(x, 61) ^ ((x) >> 6))

void SHA384_Init(SHA384_Context *ctx) {
    ctx->state[0] = 0xcbbb9d5dc1059ed8;
    ctx->state[1] = 0x629a292a367cd507;
    ctx->state[2] = 0x9159015a3070dd17;
    ctx->state[3] = 0x152fecd8f70e5939;
    ctx->state[4] = 0x67332667ffc00b31;
    ctx->state[5] = 0x8eb44a8768581511;
    ctx->state[6] = 0xdb0c2e0d64f98fa7;
    ctx->state[7] = 0x47b5481dbefa4fa4;
    ctx->bitcount = 0;
    ctx->buffer_len = 0;
}

void SHA384_Transform(SHA384_Context *ctx) {
    int i;
    uint64_t a, b, c, d, e, f, g, h;
    uint64_t w[80];
    
    for (i = 0; i < 16; i++) {
        w[i] = ((uint64_t)ctx->buffer[i*8] << 56) |
               ((uint64_t)ctx->buffer[i*8+1] << 48) |
               ((uint64_t)ctx->buffer[i*8+2] << 40) |
               ((uint64_t)ctx->buffer[i*8+3] << 32) |
               ((uint64_t)ctx->buffer[i*8+4] << 24) |
               ((uint64_t)ctx->buffer[i*8+5] << 16) |
               ((uint64_t)ctx->buffer[i*8+6] << 8) |
               ((uint64_t)ctx->buffer[i*8+7]);
    }
    
    for (i = 16; i < 80; i++) {
        w[i] = SHA384_SN1(w[i-2]) + w[i-7] + SHA384_SN0(w[i-15]) + w[i-16];
    }
    
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    for (i = 0; i < 80; i++) {
        uint64_t t1 = h + SHA384_EP1(e) + SHA384_CH(e, f, g) + SHA384_K[i] + w[i];
        uint64_t t2 = SHA384_EP0(a) + SHA384_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void SHA384_Update(SHA384_Context *ctx, const uint8_t *data, size_t len) {
    size_t i;
    
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];
        ctx->bitcount += 8;
        
        if (ctx->buffer_len == 128) {
            SHA384_Transform(ctx);
            ctx->buffer_len = 0;
        }
    }
}

void SHA384_Final(SHA384_Context *ctx, uint8_t *digest) {
    size_t i;
    uint64_t bitcount_be;
    
    ctx->buffer[ctx->buffer_len++] = 0x80;
    
    while (ctx->buffer_len != 112) {
        if (ctx->buffer_len == 128) {
            SHA384_Transform(ctx);
            ctx->buffer_len = 0;
        }
        ctx->buffer[ctx->buffer_len++] = 0;
    }
    
    for (i = 0; i < 8; i++) {
        ctx->buffer[112 + i] = 0;
    }
    
    bitcount_be = ctx->bitcount;
    for (i = 0; i < 8; i++) {
        ctx->buffer[120 + i] = (bitcount_be >> (56 - i * 8)) & 0xFF;
    }
    
    SHA384_Transform(ctx);
    
    for (i = 0; i < 6; i++) {
        digest[i*8] = (ctx->state[i] >> 56) & 0xFF;
        digest[i*8+1] = (ctx->state[i] >> 48) & 0xFF;
        digest[i*8+2] = (ctx->state[i] >> 40) & 0xFF;
        digest[i*8+3] = (ctx->state[i] >> 32) & 0xFF;
        digest[i*8+4] = (ctx->state[i] >> 24) & 0xFF;
        digest[i*8+5] = (ctx->state[i] >> 16) & 0xFF;
        digest[i*8+6] = (ctx->state[i] >> 8) & 0xFF;
        digest[i*8+7] = ctx->state[i] & 0xFF;
    }
}

std::string JiYan_Signature_SHA384(std::string input) {
    uint8_t digest[SHA384_DIGEST_SIZE];
    SHA384_Context ctx;
    
    SHA384_Init(&ctx);
    SHA384_Update(&ctx, (const uint8_t*)input.c_str(), input.length());
    SHA384_Final(&ctx, digest);
    
    std::stringstream ss;
    for (int i = 0; i < SHA384_DIGEST_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}