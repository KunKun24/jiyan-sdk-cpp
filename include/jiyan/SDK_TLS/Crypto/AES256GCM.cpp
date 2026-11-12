/* --------------------------------- 关于 -------------------------------------

                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_AES256GCM_CPP
#define JIYAN_AES256GCM_CPP

#include <vector>
#include <string>
#include <cstring>

/* AES-256-GCM（加密方式 6）
 *
 *   输入：32 字节密钥、12 字节 nonce、明文
 *   输出：密文 || tag(16)
 *
 *   实现：AES-256 分组加密复用 AES256CBC.cpp 的 KeyExpansion / EncryptBlock，
 *         这里只做 CTR 与 GHASH，不依赖 OpenSSL / libsodium。
 */

static void JiYan_Gcm_EncryptBlock(const unsigned char roundKeys[240],
                                   const unsigned char in[16],
                                   unsigned char out[16]) {
    unsigned char state[16];
    memcpy(state, in, 16);
    AES256_EncryptBlock(state, roundKeys);
    memcpy(out, state, 16);
}

/* GF(2^128) 乘法，位序按 GCM 规范（MSB first） */
static void JiYan_Gcm_Mul(const unsigned char x[16],
                          const unsigned char y[16],
                          unsigned char out[16]) {
    unsigned char z[16];
    unsigned char v[16];
    memset(z, 0, 16);
    memcpy(v, y, 16);

    for (int i = 0; i < 128; ++i) {
        if ((x[i >> 3] >> (7 - (i & 7))) & 1) {
            for (int j = 0; j < 16; ++j) z[j] ^= v[j];
        }
        const bool lsb = (v[15] & 1) != 0;
        for (int j = 15; j > 0; --j) {
            v[j] = static_cast<unsigned char>((v[j] >> 1) | ((v[j - 1] & 1) << 7));
        }
        v[0] = static_cast<unsigned char>(v[0] >> 1);
        if (lsb) v[0] ^= 0xE1;
    }
    memcpy(out, z, 16);
}

/* GHASH（AAD 固定为空，与服务端一致） */
static void JiYan_Gcm_Ghash(const unsigned char H[16],
                            const unsigned char* ct, size_t ctLen,
                            unsigned char out[16]) {
    unsigned char y[16];
    memset(y, 0, 16);

    unsigned char block[16];
    size_t i = 0;
    while (i < ctLen) {
        memset(block, 0, 16);
        const size_t n = (ctLen - i < 16) ? (ctLen - i) : 16;
        memcpy(block, ct + i, n);
        for (int j = 0; j < 16; ++j) y[j] ^= block[j];
        unsigned char tmp[16];
        JiYan_Gcm_Mul(y, H, tmp);
        memcpy(y, tmp, 16);
        i += n;
    }

    /* len(A)=0 || len(C) 以 bit 计，64 位大端 */
    memset(block, 0, 16);
    unsigned long long bits = static_cast<unsigned long long>(ctLen) * 8ULL;
    for (int j = 0; j < 8; ++j) {
        block[15 - j] = static_cast<unsigned char>((bits >> (8 * j)) & 0xFF);
    }
    for (int j = 0; j < 16; ++j) y[j] ^= block[j];
    unsigned char tmp[16];
    JiYan_Gcm_Mul(y, H, tmp);
    memcpy(out, tmp, 16);
}

/* 输出 ct || tag，长度为 plainLen + 16 */
static std::vector<unsigned char> JiYan_AES256GCM_Seal(const unsigned char key[32],
                                                      const unsigned char nonce[12],
                                                      const unsigned char* plain,
                                                      size_t plainLen,
                                                      unsigned char nonceLen) {
    (void)nonceLen; /* 固定 12 字节 */
    unsigned char roundKeys[240];
    AES256_KeyExpansion(key, roundKeys);

    unsigned char H[16];
    unsigned char zero[16];
    memset(zero, 0, 16);
    JiYan_Gcm_EncryptBlock(roundKeys, zero, H);

    unsigned char J0[16];
    memcpy(J0, nonce, 12);
    J0[12] = 0x00; J0[13] = 0x00; J0[14] = 0x00; J0[15] = 0x01;

    std::vector<unsigned char> out(plainLen + 16);
    unsigned char ctr[16];
    memcpy(ctr, J0, 16);

    size_t off = 0;
    while (off < plainLen) {
        /* 计数器 +1（最后 4 字节大端） */
        for (int j = 15; j >= 12; --j) {
            if (++ctr[j] != 0) break;
        }
        unsigned char ks[16];
        JiYan_Gcm_EncryptBlock(roundKeys, ctr, ks);
        const size_t n = (plainLen - off < 16) ? (plainLen - off) : 16;
        for (size_t j = 0; j < n; ++j) {
            out[off + j] = static_cast<unsigned char>(plain[off + j] ^ ks[j]);
        }
        off += n;
    }

    unsigned char S[16];
    JiYan_Gcm_Ghash(H, out.data(), plainLen, S);

    unsigned char EJ0[16];
    JiYan_Gcm_EncryptBlock(roundKeys, J0, EJ0);

    for (int j = 0; j < 16; ++j) {
        out[plainLen + j] = static_cast<unsigned char>(S[j] ^ EJ0[j]);
    }
    return out;
}

/* 输入 ct || tag，成功返回明文，认证失败返回空 */
static std::vector<unsigned char> JiYan_AES256GCM_Open(const unsigned char key[32],
                                                      const unsigned char nonce[12],
                                                      const unsigned char* in,
                                                      size_t inLen) {
    if (inLen < 16) return std::vector<unsigned char>();

    const size_t ctLen = inLen - 16;
    const unsigned char* ct = in;
    const unsigned char* tag = in + ctLen;

    unsigned char roundKeys[240];
    AES256_KeyExpansion(key, roundKeys);

    unsigned char H[16];
    unsigned char zero[16];
    memset(zero, 0, 16);
    JiYan_Gcm_EncryptBlock(roundKeys, zero, H);

    unsigned char J0[16];
    memcpy(J0, nonce, 12);
    J0[12] = 0x00; J0[13] = 0x00; J0[14] = 0x00; J0[15] = 0x01;

    unsigned char S[16];
    JiYan_Gcm_Ghash(H, ct, ctLen, S);

    unsigned char EJ0[16];
    JiYan_Gcm_EncryptBlock(roundKeys, J0, EJ0);

    unsigned char expect[16];
    unsigned char diff = 0;
    for (int j = 0; j < 16; ++j) {
        expect[j] = static_cast<unsigned char>(S[j] ^ EJ0[j]);
        diff |= static_cast<unsigned char>(expect[j] ^ tag[j]);
    }
    if (diff != 0) return std::vector<unsigned char>();

    std::vector<unsigned char> plain(ctLen);
    unsigned char ctr[16];
    memcpy(ctr, J0, 16);

    size_t off = 0;
    while (off < ctLen) {
        for (int j = 15; j >= 12; --j) {
            if (++ctr[j] != 0) break;
        }
        unsigned char ks[16];
        JiYan_Gcm_EncryptBlock(roundKeys, ctr, ks);
        const size_t n = (ctLen - off < 16) ? (ctLen - off) : 16;
        for (size_t j = 0; j < n; ++j) {
            plain[off + j] = static_cast<unsigned char>(ct[off + j] ^ ks[j]);
        }
        off += n;
    }
    return plain;
}

#endif
