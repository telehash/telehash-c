#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include "telehash.h"

/* SHA2 core MIT license: https://github.com/fpgaminer/strong-arm
Copyright (c) 2012-2013, fpgaminer@bitcoin-mining.com
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

- The names of contributors may not be used to endorse or promote products
derived from this software without specific prior written permission.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

static const uint32_t kk[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static inline uint32_t s0 (uint32_t a);
static inline uint32_t s1 (uint32_t a);
static inline uint32_t e0 (uint32_t a);
static inline uint32_t e1 (uint32_t a);
static void compress (uint32_t *digest, uint32_t *chunk);

/* gcc should optimize this to ROR instructions on at least X86 and ARM. */
#define ROR_C(x, n) (((x) >> (n)) | ((x) << (32 - (n))))


static inline uint32_t s0 (uint32_t a)
{
  uint32_t tmp1, tmp2;
  
  tmp1 = ROR_C (a, 7);
  tmp2 = ROR_C (a, 18);
  return tmp1 ^ tmp2 ^ (a >> 3);
}

static inline uint32_t s1 (uint32_t a)
{
  uint32_t tmp1, tmp2;
  
  tmp1 = ROR_C (a, 17);
  tmp2 = ROR_C (a, 19);
  return tmp1 ^ tmp2 ^ (a >> 10);
}

static inline uint32_t e0 (uint32_t a)
{
  uint32_t tmp1, tmp2;
  
  tmp1 = ROR_C (a, 2);
  tmp2 = ROR_C (a, 13);
  a = ROR_C (a, 22);
  return tmp1 ^ tmp2 ^ a;
}

static inline uint32_t e1 (uint32_t a)
{
  uint32_t tmp1, tmp2;
  
  tmp1 = ROR_C (a, 6);
  tmp2 = ROR_C (a, 11);
  a = ROR_C (a, 25);
  return tmp1 ^ tmp2 ^ a;
}

static void compress (uint32_t *digest, uint32_t *chunk)
{
  uint32_t a = digest[0], b = digest[1], c = digest[2], d = digest[3];
  uint32_t e = digest[4], f = digest[5], g = digest[6], h = digest[7];
  
  for (uint32_t i = 0; i < 64; ++i)
  {
    uint32_t w = chunk[i&15];
    
    if (i < 48)
      chunk[i&15] = chunk[i&15] + s0 (chunk[(i+1)&15]) + chunk[(i+9)&15] + s1 (chunk[(i+14)&15]);
    
    uint32_t t2 = e0 (a) + ((a&b)^(a&c)^(b&c));
    uint32_t t1 = h + e1 (e) + ((e&f)^((~e)&g)) + kk[i] + w;
    
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  
  digest[0] += a;
  digest[1] += b;
  digest[2] += c;
  digest[3] += d;
  digest[4] += e;
  digest[5] += f;
  digest[6] += g;
  digest[7] += h;
}

/* TODO: Need to correctly handle the case where len == 2^32-1 */
void SHA256 (uint8_t *hash, uint8_t const * msg, uint32_t len)
{
  uint32_t digest[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  
  for (uint32_t i = 0; i < (len >> 6); ++i)
  {
    uint32_t chunk[16];
    
    for (int j = 0; j < 16; ++j)
    {
      chunk[j] = ((uint32_t)*(msg++)) << 24;
      chunk[j] |= ((uint32_t)*(msg++)) << 16;
      chunk[j] |= ((uint32_t)*(msg++)) << 8;
      chunk[j] |= ((uint32_t)*(msg++));
    }
    
    compress (digest, chunk);
  }
  
  // Last chunk
  {
    uint32_t chunk[16] = {0};
    
    for (uint32_t i = 0; i < (len & 63); ++i)
      chunk[i>>2] |= ((uint32_t)*(msg++)) << ((3-(i&3)) << 3);
    
    chunk[(len >> 2)&15] ^= (uint32_t)1 << (8*(3-(len&3)) + 7);
    
    if ((len & 63) > 55)
    {
      compress (digest, chunk);
      memset (chunk, 0, 64);
    }
    
    chunk[14] = (len >> 29);
    chunk[15] = len << 3;
    compress (digest, chunk);
  }
  
  for (int i = 0; i < 8; ++i)
  {
    *(hash++) = digest[i] >> 24;
    *(hash++) = digest[i] >> 16;
    *(hash++) = digest[i] >> 8;
    *(hash++) = digest[i];
  }
}


static const uint32_t initial_state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
void SHA256_partial (uint8_t *hash, SHA256_CTX *const state, uint8_t const *src, uint32_t len, bool const first, bool const last)
{
  // First data
  if (first) {
    state->chunk_len = 0;
    state->totallen = 0;
    memmove (state->state, initial_state, 32);
    memset (state->chunk, 0, 64);
  }
  
  while (len > 0)
  {
    while ((state->chunk_len < 64) && (len > 0))
    {
      state->chunk[state->chunk_len>>2] |= ((uint32_t)*(src++)) << (8*(3-(state->chunk_len&3)));
      state->chunk_len += 1;
      --len;
      state->totallen += 1;
    }
    
    if (state->chunk_len == 64)
    {
      compress (state->state, state->chunk);
      state->chunk_len = 0;
      memset (state->chunk, 0, 64);
    }
  }
  
  // Last data
  if (last)
  {
    state->chunk[state->chunk_len>>2] |= (uint32_t)1 << (8*(3-(state->chunk_len&3)) + 7);
    
    if ((state->totallen & 63) > 55)
    {
      compress (state->state, state->chunk);
      memset (state->chunk, 0, 64);
    }
    
    state->chunk[14] = (state->totallen >> 29);
    state->chunk[15] = state->totallen << 3;
    compress (state->state, state->chunk);
    
    for (int i = 0; i < 8; ++i)
    {
      *(hash++) = state->state[i] >> 24;
      *(hash++) = state->state[i] >> 16;
      *(hash++) = state->state[i] >> 8;
      *(hash++) = state->state[i];
    }
  }
}

void SHA256_Init(SHA256_CTX * ctx)
{
  SHA256_partial(NULL, ctx, NULL, 0, true, false);
}

void SHA256_Update(SHA256_CTX * ctx, const void *in, size_t len)
{
  SHA256_partial(NULL, ctx, in, len, false, false);
}

void SHA256_Final(unsigned char digest[32], SHA256_CTX * ctx)
{
  SHA256_partial((uint8_t*)digest, ctx, NULL, 0, false, true);
}

typedef struct HMAC_SHA256Context {
  SHA256_CTX ictx;
  SHA256_CTX octx;
} HMAC_SHA256_CTX;

/* Initialize an HMAC-SHA256 operation with the given key. */
void
HMAC_SHA256_Init(HMAC_SHA256_CTX * ctx, const void * _K, size_t Klen)
{
  unsigned char pad[64];
  unsigned char khash[32];
  const unsigned char * K = _K;
  size_t i;

  /* If Klen > 64, the key is really SHA256(K). */
  if (Klen > 64) {
    SHA256_Init(&ctx->ictx);
    SHA256_Update(&ctx->ictx, K, Klen);
    SHA256_Final(khash, &ctx->ictx);
    K = khash;
    Klen = 32;
  }

  /* Inner SHA256 operation is SHA256(K xor [block of 0x36] || data). */
  SHA256_Init(&ctx->ictx);
  memset(pad, 0x36, 64);
  for (i = 0; i < Klen; i++)
    pad[i] ^= K[i];
  SHA256_Update(&ctx->ictx, pad, 64);

  /* Outer SHA256 operation is SHA256(K xor [block of 0x5c] || hash). */
  SHA256_Init(&ctx->octx);
  memset(pad, 0x5c, 64);
  for (i = 0; i < Klen; i++)
    pad[i] ^= K[i];
  SHA256_Update(&ctx->octx, pad, 64);

  /* Clean the stack. */
  memset(khash, 0, 32);
}

/* Add bytes to the HMAC-SHA256 operation. */
void
HMAC_SHA256_Update(HMAC_SHA256_CTX * ctx, const void *in, size_t len)
{

  /* Feed data to the inner SHA256 operation. */
  SHA256_Update(&ctx->ictx, in, len);
}

/* Finish an HMAC-SHA256 operation. */
void
HMAC_SHA256_Final(unsigned char digest[32], HMAC_SHA256_CTX * ctx)
{
  unsigned char ihash[32];

  /* Finish the inner SHA256 operation. */
  SHA256_Final(ihash, &ctx->ictx);

  /* Feed the inner hash to the outer SHA256 operation. */
  SHA256_Update(&ctx->octx, ihash, 32);

  /* Finish the outer SHA256 operation. */
  SHA256_Final(digest, &ctx->octx);

  /* Clean the stack. */
  memset(ihash, 0, 32);
}

void sha256( const unsigned char *input, size_t ilen,
             unsigned char output[32], int is224)
{
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, input, ilen);
  SHA256_Final(output, &ctx);
}

void sha256_hmac( const unsigned char *key, size_t keylen,
                  const unsigned char *input, size_t ilen,
                  unsigned char output[32], int is224 )
{
  HMAC_SHA256_CTX hctx;
  HMAC_SHA256_Init(&hctx, key, keylen);
  HMAC_SHA256_Update(&hctx, input, ilen);
  HMAC_SHA256_Final(output, &hctx);
}

void hmac_256(const unsigned char *key, size_t keylen, const unsigned char *input, size_t ilen, unsigned char output[32])
{
  sha256_hmac(key, keylen, input, ilen, output, 0);
}

/*
   Implements the HKDF algorithm (HMAC-based Extract-and-Expand Key
   Derivation Function, RFC 5869).
   This implementation is adapted from IETF's HKDF implementation,
   see associated license below.
*/
/*
   Copyright (c) 2011 IETF Trust and the persons identified as
   authors of the code.  All rights reserved.
   Redistribution and use in source and binary forms, with or
   without modification, are permitted provided that the following
   conditions are met:
   - Redistributions of source code must retain the above
     copyright notice, this list of conditions and
     the following disclaimer.
   - Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.
   - Neither the name of Internet Society, IETF or IETF Trust, nor
     the names of specific contributors, may be used to endorse or
     promote products derived from this software without specific
     prior written permission.
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define SHA256_DIGEST_SIZE 32

/*
 *  hkdf_sha256_extract
 *
 *  Description:
 *      This function will perform HKDF extraction with SHA256.
 *
 *  Parameters:
 *      salt: [in]
 *          The optional salt value (a non-secret random value);
 *          if not provided (salt == NULL), it is set internally
 *          to a string of HashLen(hmac_algorithm) zeros.
 *      salt_len: [in]
 *          The length of the salt value.  (Ignored if salt == NULL.)
 *      ikm: [in]
 *          Input keying material.
 *      ikm_len: [in]
 *          The length of the input keying material.
 *      prk: [out]
 *          Array where the HKDF extraction is to be stored.
 *          Must be euqual to or larger than 32 bytes
 *
 *  Returns:
 *      0 on success, -1 on error.
 *
 */
static int hkdf_sha256_extract(
uint8_t  *salt,
uint32_t      salt_len,
uint8_t  *ikm,
uint32_t      ikm_len,
uint8_t       *prk
)
{
  uint8_t null_salt[SHA256_DIGEST_SIZE];

  if (ikm == NULL)
  {
    LOG_DEBUG("Error: incorrect input parameter for hkdf_sha256_extract");
    return -1;
  }

  if (salt == NULL) {
    salt = null_salt;
    salt_len = SHA256_DIGEST_SIZE;
    memset(null_salt, '\0', salt_len);
  }

  hmac_256(salt, salt_len,
              ikm,  ikm_len,
              prk);
  return 0;
}

/*
 *  hkdf_sha256_expand
 *
 *  Description:
 *      This function will perform HKDF with SHA256 expansion.
 *
 *  Parameters:
 *      prk: [in]
 *          The pseudo-random key to be expanded; either obtained
 *          directly from a cryptographically strong, uniformly
 *          distributed pseudo-random number generator, or as the
 *          output from hkdfExtract().
 *      prk_len: [in]
 *          The length of the pseudo-random key in prk;
 *          should at least be equal to HashSize(hmac_algorithm).
 *      info: [in]
 *          The optional context and application specific information.
 *          If info == NULL or a zero-length string, it is ignored.
 *      info_len: [in]
 *          The length of the optional context and application specific
 *          information. (Ignored if info == NULL.)
 *      okm: [out]
 *          Where the HKDF is to be stored.
 *      okm_len: [in]
 *          The length of the buffer to hold okm.
 *          okm_len must be <= 255 * SHA256_DIGEST_SIZE
 *
 *  Returns:
 *      0 on success, -1 on error.
 *
 */
static int hkdf_sha256_expand
(
uint8_t   prk[],
uint32_t      prk_len,
uint8_t   *info,
uint32_t      info_len,
uint8_t       okm[],
uint32_t      okm_len
)
{
  uint32_t hash_len;
  uint32_t N;
  uint8_t T[SHA256_DIGEST_SIZE];
  uint32_t Tlen;
  uint32_t pos;
  uint32_t i;
  HMAC_SHA256_CTX* pctx;
  uint8_t c;

  if( ( prk_len == 0 ) || ( okm_len == 0 ) || ( okm == NULL ) )
  {
      LOG_DEBUG("Error: incorrect input parameter for hkdf_sha256");
    return -1;
  }

  pctx = malloc(sizeof(HMAC_SHA256_CTX));
  if(NULL == pctx)
  {
      LOG_DEBUG("Error: fail to allocate memory for hmac_sha256");
      return -2;
  }

  if (info == NULL) {
    info = (uint8_t *)"";
    info_len = 0;
  }

  hash_len = SHA256_DIGEST_SIZE;
  if (prk_len < hash_len)
  {
      LOG_DEBUG("Error: prk size (%d) is smaller than hash size (%d)", prk_len, hash_len);
      return -3;
  }
  N = okm_len / hash_len;
  if (okm_len % hash_len)
    N++;
  if (N > 255)
  {
      LOG_DEBUG("Error: incorrect input size (%d) for hkdf_sha256", N);
      free(pctx);
      return -4;
  }

  Tlen = 0;
  pos = 0;
  for (i = 1; i <= N; i++) {
    c = i;
    memset( pctx, 0, sizeof(HMAC_SHA256_CTX) );

    HMAC_SHA256_Init(pctx, prk, prk_len);
    HMAC_SHA256_Update(pctx, T, Tlen);
    HMAC_SHA256_Update(pctx, info, info_len);
    HMAC_SHA256_Update(pctx, &c, 1);
    HMAC_SHA256_Final(T, pctx);

    memcpy(okm + pos, T, (i != N) ? hash_len : (okm_len - pos));
    pos += hash_len;
    Tlen = hash_len;
  }
  memset( pctx, 0, sizeof(HMAC_SHA256_CTX) );
  free(pctx);
  return 0;
}

/*
 *  hkdf_sha256
 *
 *  Description:
 *      This function will generate keying material using HKDF with SHA256.
 *
 *  Parameters:
 *      salt: [in]
 *          The optional salt value (a non-secret random value);
 *          if not provided (salt == NULL), it is set internally
 *          to a string of HashLen(hmac_algorithm) zeros.
 *      salt_len: [in]
 *          The length of the salt value. (Ignored if salt == NULL.)
 *      ikm: [in]
 *          Input keying material.
 *      ikm_len: [in]
 *          The length of the input keying material.
 *      info: [in]
 *          The optional context and application specific information.
 *          If info == NULL or a zero-length string, it is ignored.
 *      info_len: [in]
 *          The length of the optional context and application specific
 *          information. (Ignored if info == NULL.)
 *      okm: [out]
 *          Where the HKDF is to be stored.
 *      okm_len: [in]
 *          The length of the buffer to hold okm.
 *          okm_len must be <= 255 * 32
 *
 *  Notes:
 *      Calls hkdfExtract() and hkdfExpand().
 *
 *  Returns:
 *      0 on success, <0 on error.
 *
 */
int hkdf_sha256( uint8_t *salt, uint32_t salt_len,
    uint8_t *ikm, uint32_t ikm_len,
    uint8_t *info, uint32_t info_len,
    uint8_t *okm, uint32_t okm_len)
{
  int32_t i_ret;
  uint8_t prk[SHA256_DIGEST_SIZE];

  i_ret = hkdf_sha256_extract(salt, salt_len, ikm, ikm_len, prk);
  if(0 != i_ret)
  {
    LOG_DEBUG("Error: fail to execute hkdf_sha256_extract()");
    return -1;
  }

  i_ret =  hkdf_sha256_expand(prk, SHA256_DIGEST_SIZE, info, info_len, okm, okm_len);
  if(0 != i_ret)
  {
    LOG_DEBUG("Error: fail to execute hkdf_sha256_expand()");
    return -2;
  }

  return 0;
}

