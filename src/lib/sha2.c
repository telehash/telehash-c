#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "sha256.h"

#define SHA256_STATE SHA256_CTX



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
void SHA256_partial (uint8_t *hash, SHA256_STATE *const state, uint8_t const *src, uint32_t len, bool const first, bool const last)
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
