/* --------------------------------- 关于 -------------------------------------

                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_BLAKE2B_CPP
#define JIYAN_BLAKE2B_CPP

#include <string>
#include <cstring>
#include <cstdint>

/* BLAKE2b-256（签名方式 14，带密钥）
 *
 *   key = SHA-256(sign_key) 的原始 32 字节（与服务端一致）
 *   输出：32 字节摘要的小写十六进制（64 字符）
 */

static inline std::uint64_t JiYan_B2b_Load64LE(const unsigned char* p) {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

static inline void JiYan_B2b_Store64LE(unsigned char* p, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}

static inline std::uint64_t JiYan_B2b_Rotr64(std::uint64_t v, int n) {
    return (v >> n) | (v << (64 - n));
}

static const std::uint64_t JiYan_B2b_IV[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const unsigned char JiYan_B2b_Sigma[12][16] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};

static inline void JiYan_B2b_G(std::uint64_t v[16], int a, int b, int c, int d,
                               std::uint64_t x, std::uint64_t y) {
    v[a] = v[a] + v[b] + x;
    v[d] = JiYan_B2b_Rotr64(v[d] ^ v[a], 32);
    v[c] = v[c] + v[d];
    v[b] = JiYan_B2b_Rotr64(v[b] ^ v[c], 24);
    v[a] = v[a] + v[b] + y;
    v[d] = JiYan_B2b_Rotr64(v[d] ^ v[a], 16);
    v[c] = v[c] + v[d];
    v[b] = JiYan_B2b_Rotr64(v[b] ^ v[c], 63);
}

struct JiYan_Blake2b_State {
    std::uint64_t h[8];
    std::uint64_t t[2];
    unsigned char buf[128];
    size_t buflen;
    size_t outlen;
};

static void JiYan_B2b_Init(JiYan_Blake2b_State* S, size_t outlen, size_t keylen) {
    for (int i = 0; i < 8; ++i) S->h[i] = JiYan_B2b_IV[i];
    S->h[0] ^= 0x01010000ULL ^ (static_cast<std::uint64_t>(keylen) << 8) ^ static_cast<std::uint64_t>(outlen);
    S->t[0] = S->t[1] = 0;
    S->buflen = 0;
    S->outlen = outlen;
    memset(S->buf, 0, sizeof(S->buf));
}

static void JiYan_B2b_Compress(JiYan_Blake2b_State* S, const unsigned char block[128], bool last) {
    std::uint64_t m[16];
    for (int i = 0; i < 16; ++i) m[i] = JiYan_B2b_Load64LE(block + i * 8);

    std::uint64_t v[16];
    for (int i = 0; i < 8; ++i) v[i] = S->h[i];
    for (int i = 0; i < 8; ++i) v[8 + i] = JiYan_B2b_IV[i];
    v[12] ^= S->t[0];
    v[13] ^= S->t[1];
    if (last) v[14] = ~v[14];

    for (int r = 0; r < 12; ++r) {
        const unsigned char* s = JiYan_B2b_Sigma[r];
        JiYan_B2b_G(v, 0, 4,  8, 12, m[s[0]],  m[s[1]]);
        JiYan_B2b_G(v, 1, 5,  9, 13, m[s[2]],  m[s[3]]);
        JiYan_B2b_G(v, 2, 6, 10, 14, m[s[4]],  m[s[5]]);
        JiYan_B2b_G(v, 3, 7, 11, 15, m[s[6]],  m[s[7]]);
        JiYan_B2b_G(v, 0, 5, 10, 15, m[s[8]],  m[s[9]]);
        JiYan_B2b_G(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
        JiYan_B2b_G(v, 2, 7,  8, 13, m[s[12]], m[s[13]]);
        JiYan_B2b_G(v, 3, 4,  9, 14, m[s[14]], m[s[15]]);
    }
    for (int i = 0; i < 8; ++i) S->h[i] ^= v[i] ^ v[8 + i];
}

static void JiYan_B2b_Increment(JiYan_Blake2b_State* S, std::uint64_t n) {
    S->t[0] += n;
    if (S->t[0] < n) ++S->t[1];
}

static void JiYan_B2b_Update(JiYan_Blake2b_State* S, const unsigned char* in, size_t len) {
    if (len == 0) return;

    /* 缓冲区已满且还有新数据：说明这一块不是最后一块，先按非最终块压缩 */
    if (S->buflen == 128) {
        JiYan_B2b_Increment(S, 128);
        JiYan_B2b_Compress(S, S->buf, false);
        S->buflen = 0;
    }

    if (S->buflen + len > 128) {
        const size_t fill = 128 - S->buflen;
        memcpy(S->buf + S->buflen, in, fill);
        JiYan_B2b_Increment(S, 128);
        JiYan_B2b_Compress(S, S->buf, false);
        S->buflen = 0;
        in += fill;
        len -= fill;
        while (len > 128) {
            JiYan_B2b_Increment(S, 128);
            JiYan_B2b_Compress(S, in, false);
            in += 128;
            len -= 128;
        }
    }
    memcpy(S->buf + S->buflen, in, len);
    S->buflen += len;
}

static void JiYan_B2b_Final(JiYan_Blake2b_State* S, unsigned char out[32]) {
    JiYan_B2b_Increment(S, static_cast<std::uint64_t>(S->buflen));
    memset(S->buf + S->buflen, 0, 128 - S->buflen);
    JiYan_B2b_Compress(S, S->buf, true);

    unsigned char full[64];
    for (int i = 0; i < 8; ++i) JiYan_B2b_Store64LE(full + i * 8, S->h[i]);
    memcpy(out, full, 32);
}

/* 带密钥 BLAKE2b-256，返回 32 字节原始摘要 */
static std::string JiYan_Blake2b_256_Keyed(const std::string& msg, const std::string& key32) {
    JiYan_Blake2b_State S;
    const size_t keylen = key32.size() > 32 ? 32 : key32.size();
    JiYan_B2b_Init(&S, 32, keylen);

    if (keylen > 0) {
        unsigned char keyblock[128];
        memset(keyblock, 0, sizeof(keyblock));
        memcpy(keyblock, key32.data(), keylen);
        JiYan_B2b_Update(&S, keyblock, 128);
    }
    if (!msg.empty()) {
        JiYan_B2b_Update(&S, reinterpret_cast<const unsigned char*>(msg.data()), msg.size());
    }

    unsigned char digest[32];
    JiYan_B2b_Final(&S, digest);
    return std::string(reinterpret_cast<const char*>(digest), 32);
}

static std::string JiYan_Blake2b_256_Keyed_Hex(const std::string& msg, const std::string& key32) {
    const std::string raw = JiYan_Blake2b_256_Keyed(msg, key32);
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(raw.size() * 2);
    for (size_t i = 0; i < raw.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0x0F]);
    }
    return out;
}

#endif
