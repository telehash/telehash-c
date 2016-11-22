/*-
 * Copyright 2005,2007,2009 Colin Percival
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include "telehash.h"


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

