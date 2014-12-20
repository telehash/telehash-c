#ifndef murmur_h
#define murmur_h

#include <stdint.h>

// murmurhash3 32bit
uint32_t murmur4(const uint32_t * data, uint8_t len);
// hex must be 8+\0
char *murmur8(const uint32_t* data, uint8_t len, char *hex);

#endif
