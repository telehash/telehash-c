#ifndef util_h
#define util_h

#include <stdint.h>

// make sure out is 2*len + 1
unsigned char *util_hex(unsigned char *in, int len, unsigned char *out);
// out must be len/2
unsigned char *util_unhex(unsigned char *in, int len, unsigned char *out);
// hex string validator, NULL is invalid, else returns str
char *util_ishex(char *str, int len);

// safer string comparison (0 == same)
int util_cmp(char *a, char *b);

// murmurhash3 32bit
uint32_t util_mmh32(const uint8_t * data, int len);
// hex must be 9
char *util_murmur(const unsigned char* data, int len, char *hex);

// portable sort
void util_sort(void *base, int nel, int width, int (*comp)(void *, const void *, const void *), void *arg);

// portable reallocf
void *util_reallocf(void *ptr, size_t size);

#endif