#ifndef cs1a_hmac_h
#define cs1a_hmac_h

#include <stddef.h>

void hmac_256(const unsigned char *key, size_t keylen,
                  const unsigned char *input, size_t ilen,
                  unsigned char output[32]);

#endif
