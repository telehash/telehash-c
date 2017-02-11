// local wrappers/additions
#include <stdint.h>

// murmurhash3 32bit
uint32_t murmur4(const uint8_t *data, uint32_t len);

// hex must be 8+\0
char *murmur8(const uint8_t *data, uint32_t len, char *hex);

// more convenient, caller must ensure 4-byte size of hash output
uint8_t *murmur(uint32_t seed, const uint8_t *data, uint32_t len, uint8_t *hash);


/*-----------------------------------------------------------------------------
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain.
 *
 * This implementation was written by Shane Day, and is also public domain.
 *
 * This is a portable ANSI C implementation of MurmurHash3_x86_32 (Murmur3A)
 * with support for progressive processing.
 */


/* ------------------------------------------------------------------------- */
/* Prototypes */

#ifdef __cplusplus
extern "C" {
#endif

void PMurHash32_Process(uint32_t *ph1, uint32_t *pcarry, const void *key, int len);
uint32_t PMurHash32_Result(uint32_t h1, uint32_t carry, uint32_t total_length);
uint32_t PMurHash32(uint32_t seed, const void *key, int len);

void PMurHash32_test(const void *key, int len, uint32_t seed, void *out);

#ifdef __cplusplus
}
#endif
