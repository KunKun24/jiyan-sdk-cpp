/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#define SHA3_512_BLOCK_SIZE     72
#define SHA3_512_DIGEST_SIZE    64
#define SHA3_512_RATE           72

typedef struct {
    uint64_t state[25];
    uint8_t buffer[SHA3_512_RATE];
    size_t buffer_len;
    size_t size;
} SHA3_512_Context;

static const uint64_t SHA3_512_keccakf_rndc[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
    0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
    0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
    0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
    0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
    0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

static const int SHA3_512_keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

static const int SHA3_512_keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

static uint64_t SHA3_512_rol64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

void SHA3_512_keccak_f1600(uint64_t state[25]) {
    int round, i, x, y;
    uint64_t bc[5], t;
    
    for (round = 0; round < 24; round++) {
        for (x = 0; x < 5; x++) {
            bc[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        }
        
        for (x = 0; x < 5; x++) {
            t = bc[(x + 4) % 5] ^ SHA3_512_rol64(bc[(x + 1) % 5], 1);
            for (y = 0; y < 25; y += 5) {
                state[x + y] ^= t;
            }
        }
        
        t = state[1];
        for (i = 0; i < 24; i++) {
            int j = SHA3_512_keccakf_piln[i];
            bc[0] = state[j];
            state[j] = SHA3_512_rol64(t, SHA3_512_keccakf_rotc[i]);
            t = bc[0];
        }
        
        for (y = 0; y < 25; y += 5) {
            for (x = 0; x < 5; x++) {
                bc[x] = state[x + y];
            }
            for (x = 0; x < 5; x++) {
                state[x + y] = bc[x] ^ ((~bc[(x + 1) % 5]) & bc[(x + 2) % 5]);
            }
        }
        
        state[0] ^= SHA3_512_keccakf_rndc[round];
    }
}

void SHA3_512_Init(SHA3_512_Context *ctx) {
    memset(ctx->state, 0, sizeof(ctx->state));
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
    ctx->buffer_len = 0;
    ctx->size = 0;
}

void SHA3_512_Absorb(SHA3_512_Context *ctx, const uint8_t *data, size_t len) {
    size_t i, j;
    
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];
        ctx->size++;
        
        if (ctx->buffer_len == SHA3_512_RATE) {
            for (j = 0; j < SHA3_512_RATE; j += 8) {
                ctx->state[j / 8] ^= *(uint64_t*)(ctx->buffer + j);
            }
            SHA3_512_keccak_f1600(ctx->state);
            ctx->buffer_len = 0;
        }
    }
}

void SHA3_512_Final(SHA3_512_Context *ctx, uint8_t *digest) {
    size_t j;
    
    ctx->buffer[ctx->buffer_len++] = 0x06;
    
    while (ctx->buffer_len < SHA3_512_RATE) {
        ctx->buffer[ctx->buffer_len++] = 0x00;
    }
    ctx->buffer[SHA3_512_RATE - 1] |= 0x80;
    
    for (j = 0; j < SHA3_512_RATE; j += 8) {
        ctx->state[j / 8] ^= *(uint64_t*)(ctx->buffer + j);
    }
    SHA3_512_keccak_f1600(ctx->state);
    
    for (j = 0; j < SHA3_512_DIGEST_SIZE; j++) {
        digest[j] = (ctx->state[j / 8] >> (8 * (j % 8))) & 0xFF;
    }
}

std::string JiYan_Signature_SHA3512(std::string input) {
    uint8_t digest[SHA3_512_DIGEST_SIZE];
    SHA3_512_Context ctx;
    
    SHA3_512_Init(&ctx);
    SHA3_512_Absorb(&ctx, (const uint8_t*)input.c_str(), input.length());
    SHA3_512_Final(&ctx, digest);
    
    std::stringstream ss;
    for (int i = 0; i < SHA3_512_DIGEST_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}