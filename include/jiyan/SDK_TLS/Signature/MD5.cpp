/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef ENV_MD5_H
#define ENV_MD5_H
#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <sstream>



typedef struct
{
  unsigned int Env_count[2];
  unsigned int Env_state[4];
  unsigned char Env_buffer[64];
} JiYan_MD5_CTX;

#define Env_F(x, y, z) ((x & y) | (~x & z))
#define Env_G(x, y, z) ((x & z) | (y & ~z))
#define Env_H(x, y, z) (x ^ y ^ z)
#define Env_I(x, y, z) (y ^ (x | ~z))
#define Env_ROTATE_LEFT(x, n) ((x << n) | (x >> (32 - n)))
#define Env_FF(a, b, c, d, x, s, ac) \
  {                              \
    a += Env_F(b, c, d) + x + ac;    \
    a = Env_ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define Env_GG(a, b, c, d, x, s, ac) \
  {                              \
    a += Env_G(b, c, d) + x + ac;    \
    a = Env_ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define Env_HH(a, b, c, d, x, s, ac) \
  {                              \
    a += Env_H(b, c, d) + x + ac;    \
    a = Env_ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define Env_II(a, b, c, d, x, s, ac) \
  {                              \
    a += Env_I(b, c, d) + x + ac;    \
    a = Env_ROTATE_LEFT(a, s);       \
    a += b;                      \
  }

void JiYan_MD5Init(JiYan_MD5_CTX *context);
void JiYan_MD5Update(JiYan_MD5_CTX *context, unsigned char *input, unsigned int inputlen);
void JiYan_MD5Final(JiYan_MD5_CTX *context, unsigned char digest[16]);
void JiYan_MD5Transform(unsigned int state[4], unsigned char block[64]);
void JiYan_MD5Encode(unsigned char *output, unsigned int *input, unsigned int len);
void JiYan_MD5Decode(unsigned int *output, unsigned char *input, unsigned int len);

#endif

#include <string.h>

unsigned char Env_PADDING[] = {0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void JiYan_MD5Init(JiYan_MD5_CTX *context)
{
  context->Env_count[0] = 0;
  context->Env_count[1] = 0;
  context->Env_state[0] = 0x67452301;
  context->Env_state[1] = 0xEFCDAB89;
  context->Env_state[2] = 0x98BADCFE;
  context->Env_state[3] = 0x10325476;
}

void JiYan_MD5Update(JiYan_MD5_CTX *context, unsigned char *input, unsigned int inputlen)
{
  unsigned int i = 0, index = 0, partlen = 0;
  index = (context->Env_count[0] >> 3) & 0x3F;
  partlen = 64 - index;
  context->Env_count[0] += inputlen << 3;
  if (context->Env_count[0] < (inputlen << 3))
    context->Env_count[1]++;
  context->Env_count[1] += inputlen >> 29;

  if (inputlen >= partlen)
  {
    memcpy(&context->Env_buffer[index], input, partlen);
    JiYan_MD5Transform(context->Env_state, context->Env_buffer);
    for (i = partlen; i + 64 <= inputlen; i += 64)
      JiYan_MD5Transform(context->Env_state, &input[i]);
    index = 0;
  }
  else
  {
    i = 0;
  }
  memcpy(&context->Env_buffer[index], &input[i], inputlen - i);
}

void JiYan_MD5Final(JiYan_MD5_CTX *context, unsigned char digest[16])
{
  unsigned int index = 0, padlen = 0;
  unsigned char bits[8];
  index = (context->Env_count[0] >> 3) & 0x3F;
  padlen = (index < 56) ? (56 - index) : (120 - index);
  JiYan_MD5Encode(bits, context->Env_count, 8);
  JiYan_MD5Update(context, Env_PADDING, padlen);
  JiYan_MD5Update(context, bits, 8);
  JiYan_MD5Encode(digest, context->Env_state, 16);
}

void JiYan_MD5Encode(unsigned char *output, unsigned int *input, unsigned int len)
{
  unsigned int i = 0, j = 0;
  while (j < len)
  {
    output[j] = input[i] & 0xFF;
    output[j + 1] = (input[i] >> 8) & 0xFF;
    output[j + 2] = (input[i] >> 16) & 0xFF;
    output[j + 3] = (input[i] >> 24) & 0xFF;
    i++;
    j += 4;
  }
}

void JiYan_MD5Decode(unsigned int *output, unsigned char *input, unsigned int len)
{
  unsigned int i = 0, j = 0;
  while (j < len)
  {
    output[i] = (input[j]) |
                (input[j + 1] << 8) |
                (input[j + 2] << 16) |
                (input[j + 3] << 24);
    i++;
    j += 4;
  }
}

void JiYan_MD5Transform(unsigned int state[4], unsigned char block[64])
{
  unsigned int a = state[0];
  unsigned int b = state[1];
  unsigned int c = state[2];
  unsigned int d = state[3];
  unsigned int x[64];
  JiYan_MD5Decode(x, block, 64);
  
  Env_FF(a, b, c, d, x[0], 7, 0xd76aa478);
  Env_FF(d, a, b, c, x[1], 12, 0xe8c7b756);
  Env_FF(c, d, a, b, x[2], 17, 0x242070db);
  Env_FF(b, c, d, a, x[3], 22, 0xc1bdceee);
  Env_FF(a, b, c, d, x[4], 7, 0xf57c0faf);
  Env_FF(d, a, b, c, x[5], 12, 0x4787c62a);
  Env_FF(c, d, a, b, x[6], 17, 0xa8304613);
  Env_FF(b, c, d, a, x[7], 22, 0xfd469501);
  Env_FF(a, b, c, d, x[8], 7, 0x698098d8);
  Env_FF(d, a, b, c, x[9], 12, 0x8b44f7af);
  Env_FF(c, d, a, b, x[10], 17, 0xffff5bb1);
  Env_FF(b, c, d, a, x[11], 22, 0x895cd7be);
  Env_FF(a, b, c, d, x[12], 7, 0x6b901122);
  Env_FF(d, a, b, c, x[13], 12, 0xfd987193);
  Env_FF(c, d, a, b, x[14], 17, 0xa679438e);
  Env_FF(b, c, d, a, x[15], 22, 0x49b40821);

  Env_GG(a, b, c, d, x[1], 5, 0xf61e2562);
  Env_GG(d, a, b, c, x[6], 9, 0xc040b340);
  Env_GG(c, d, a, b, x[11], 14, 0x265e5a51);
  Env_GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
  Env_GG(a, b, c, d, x[5], 5, 0xd62f105d);
  Env_GG(d, a, b, c, x[10], 9, 0x2441453);
  Env_GG(c, d, a, b, x[15], 14, 0xd8a1e681);
  Env_GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
  Env_GG(a, b, c, d, x[9], 5, 0x21e1cde6);
  Env_GG(d, a, b, c, x[14], 9, 0xc33707d6);
  Env_GG(c, d, a, b, x[3], 14, 0xf4d50d87);
  Env_GG(b, c, d, a, x[8], 20, 0x455a14ed);
  Env_GG(a, b, c, d, x[13], 5, 0xa9e3e905);
  Env_GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
  Env_GG(c, d, a, b, x[7], 14, 0x676f02d9);
  Env_GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

  Env_HH(a, b, c, d, x[5], 4, 0xfffa3942);
  Env_HH(d, a, b, c, x[8], 11, 0x8771f681);
  Env_HH(c, d, a, b, x[11], 16, 0x6d9d6122);
  Env_HH(b, c, d, a, x[14], 23, 0xfde5380c);
  Env_HH(a, b, c, d, x[1], 4, 0xa4beea44);
  Env_HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
  Env_HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
  Env_HH(b, c, d, a, x[10], 23, 0xbebfbc70);
  Env_HH(a, b, c, d, x[13], 4, 0x289b7ec6);
  Env_HH(d, a, b, c, x[0], 11, 0xeaa127fa);
  Env_HH(c, d, a, b, x[3], 16, 0xd4ef3085);
  Env_HH(b, c, d, a, x[6], 23, 0x4881d05);
  Env_HH(a, b, c, d, x[9], 4, 0xd9d4d039);
  Env_HH(d, a, b, c, x[12], 11, 0xe6db99e5);
  Env_HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
  Env_HH(b, c, d, a, x[2], 23, 0xc4ac5665);

  Env_II(a, b, c, d, x[0], 6, 0xf4292244);
  Env_II(d, a, b, c, x[7], 10, 0x432aff97);
  Env_II(c, d, a, b, x[14], 15, 0xab9423a7);
  Env_II(b, c, d, a, x[5], 21, 0xfc93a039);
  Env_II(a, b, c, d, x[12], 6, 0x655b59c3);
  Env_II(d, a, b, c, x[3], 10, 0x8f0ccc92);
  Env_II(c, d, a, b, x[10], 15, 0xffeff47d);
  Env_II(b, c, d, a, x[1], 21, 0x85845dd1);
  Env_II(a, b, c, d, x[8], 6, 0x6fa87e4f);
  Env_II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
  Env_II(c, d, a, b, x[6], 15, 0xa3014314);
  Env_II(b, c, d, a, x[13], 21, 0x4e0811a1);
  Env_II(a, b, c, d, x[4], 6, 0xf7537e82);
  Env_II(d, a, b, c, x[11], 10, 0xbd3af235);
  Env_II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
  Env_II(b, c, d, a, x[9], 21, 0xeb86d391);
  
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

std::string JiYan_Signature_MD5(std::string input) {
    JiYan_MD5_CTX context;
    JiYan_MD5Init(&context);
    unsigned char digest[16];
    JiYan_MD5Update(&context, (unsigned char*)input.c_str(), input.length());
    
    JiYan_MD5Final(&context, digest);
    std::stringstream ss;
    for(int i = 0; i < 16; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    
    return ss.str();
}