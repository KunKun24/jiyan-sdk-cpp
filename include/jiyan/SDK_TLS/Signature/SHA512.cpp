/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

class SHA512 {
public:
    SHA512() {
        reset();
    }
    
    void reset() {
        h[0] = 0x6a09e667f3bcc908ULL;
        h[1] = 0xbb67ae8584caa73bULL;
        h[2] = 0x3c6ef372fe94f82bULL;
        h[3] = 0xa54ff53a5f1d36f1ULL;
        h[4] = 0x510e527fade682d1ULL;
        h[5] = 0x9b05688c2b3e6c1fULL;
        h[6] = 0x1f83d9abfb41bd6bULL;
        h[7] = 0x5be0cd19137e2179ULL;
        
        buffer_len = 0;
        total_len_high = 0;
        total_len_low = 0;
        finalized = false;
    }

    void update(const uint8_t* data, size_t length) {
        if (finalized) {
            reset();
        }

        uint64_t temp_low = total_len_low;
        uint64_t temp_high = total_len_high;
        
        total_len_low += (static_cast<uint64_t>(length) & 0xFFFFFFFFFFFFFFFFULL);

        if (total_len_low < temp_low) {
            total_len_high++;
        }
        
        total_len_high += (static_cast<uint64_t>(length) >> 32 >> 32);

        while (length > 0) {
            size_t fill = 128 - buffer_len;
            size_t to_copy = (length < fill) ? length : fill;
            
            std::copy(data, data + to_copy, buffer + buffer_len);
            buffer_len += to_copy;
            data += to_copy;
            length -= to_copy;
            
            if (buffer_len == 128) {
                transform(buffer);
                buffer_len = 0;
            }
        }
    }
    
    void update(const std::string& data) {
        update(reinterpret_cast<const uint8_t*>(data.data()), data.length());
    }
    
    std::string digest() {
        if (!finalized) {
            finalize();
        }
        
        std::stringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(16) << h[i];
        }
        return ss.str();
    }
    
    std::vector<uint8_t> digest_bytes() {
        if (!finalized) {
            finalize();
        }
        
        std::vector<uint8_t> result(64);
        for (int i = 0; i < 8; ++i) {
            result[i * 8] = (h[i] >> 56) & 0xFF;
            result[i * 8 + 1] = (h[i] >> 48) & 0xFF;
            result[i * 8 + 2] = (h[i] >> 40) & 0xFF;
            result[i * 8 + 3] = (h[i] >> 32) & 0xFF;
            result[i * 8 + 4] = (h[i] >> 24) & 0xFF;
            result[i * 8 + 5] = (h[i] >> 16) & 0xFF;
            result[i * 8 + 6] = (h[i] >> 8) & 0xFF;
            result[i * 8 + 7] = h[i] & 0xFF;
        }
        return result;
    }
    
    static std::string hash(const std::string& input) {
        SHA512 sha;
        sha.update(input);
        return sha.digest();
    }
    
    static std::vector<uint8_t> hash_bytes(const std::string& input) {
        SHA512 sha;
        sha.update(input);
        return sha.digest_bytes();
    }

private:
    uint64_t h[8];
    
    uint8_t buffer[128];
    size_t buffer_len;
    uint64_t total_len_low;
    uint64_t total_len_high;
    bool finalized;
    
    static const uint64_t k[80];
    
    void transform(const uint8_t* chunk) {
        uint64_t w[80];
        
        for (int i = 0; i < 16; ++i) {
            w[i] = 
                (static_cast<uint64_t>(chunk[i * 8]) << 56) |
                (static_cast<uint64_t>(chunk[i * 8 + 1]) << 48) |
                (static_cast<uint64_t>(chunk[i * 8 + 2]) << 40) |
                (static_cast<uint64_t>(chunk[i * 8 + 3]) << 32) |
                (static_cast<uint64_t>(chunk[i * 8 + 4]) << 24) |
                (static_cast<uint64_t>(chunk[i * 8 + 5]) << 16) |
                (static_cast<uint64_t>(chunk[i * 8 + 6]) << 8) |
                static_cast<uint64_t>(chunk[i * 8 + 7]);
        }
        
        for (int i = 16; i < 80; ++i) {
            w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
        }
        
        uint64_t a = h[0];
        uint64_t b = h[1];
        uint64_t c = h[2];
        uint64_t d = h[3];
        uint64_t e = h[4];
        uint64_t f = h[5];
        uint64_t g = h[6];
        uint64_t h_val = h[7];
        
        for (int i = 0; i < 80; ++i) {
            uint64_t t1 = h_val + sum_e1(e) + ch(e, f, g) + k[i] + w[i];
            uint64_t t2 = sum_e0(a) + maj(a, b, c);
            h_val = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += h_val;
    }
    
    void finalize() {
        if (finalized) return;
        
        uint64_t bit_len_low = total_len_low << 3;
        uint64_t bit_len_high = (total_len_high << 3) | (total_len_low >> 61);
        
        buffer[buffer_len++] = 0x80;
        
        if (buffer_len > 112) {
            while (buffer_len < 128) {
                buffer[buffer_len++] = 0;
            }
            transform(buffer);
            buffer_len = 0;
        }
        
        while (buffer_len < 112) {
            buffer[buffer_len++] = 0;
        }
        
        for (int i = 0; i < 8; ++i) {
            buffer[119 - i] = (bit_len_high >> (i * 8)) & 0xFF;
        }
        for (int i = 0; i < 8; ++i) {
            buffer[127 - i] = (bit_len_low >> (i * 8)) & 0xFF;
        }
        
        transform(buffer);
        finalized = true;
    }
    
    static uint64_t rotr(uint64_t x, uint64_t n) {
        return (x >> n) | (x << (64 - n));
    }
    
    static uint64_t ch(uint64_t x, uint64_t y, uint64_t z) {
        return (x & y) ^ (~x & z);
    }
    
    static uint64_t maj(uint64_t x, uint64_t y, uint64_t z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }
    
    static uint64_t sum_e0(uint64_t x) {
        return rotr(x, 28) ^ rotr(x, 34) ^ rotr(x, 39);
    }
    
    static uint64_t sum_e1(uint64_t x) {
        return rotr(x, 14) ^ rotr(x, 18) ^ rotr(x, 41);
    }
    
    static uint64_t sigma0(uint64_t x) {
        return rotr(x, 1) ^ rotr(x, 8) ^ (x >> 7);
    }
    
    static uint64_t sigma1(uint64_t x) {
        return rotr(x, 19) ^ rotr(x, 61) ^ (x >> 6);
    }
};

const uint64_t SHA512::k[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};


std::string JiYan_Signature_SHA512(const std::string& input) {
    return SHA512::hash(input);
}
