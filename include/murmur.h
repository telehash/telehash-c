#ifndef murmur_h
#define murmur_h

#include <stdint.h>

// murmurhash3 32bit
uint32_t murmur4(const uint32_t * data, uint32_t len);

// hex must be 8+\0
char *murmur8(const uint32_t* data, uint32_t len, char *hex);

// more convenient, caller must ensure 4-byte sizing
uint8_t *murmur(const uint8_t *data, uint32_t len, uint8_t *hash);

#endif
