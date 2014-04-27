#ifndef cs1a_aes_h
#define cs1a_aes_h

#include <stddef.h>

void aes_128_ctr(unsigned char *key, size_t length, unsigned char nonce_counter[16], const unsigned char *input, unsigned char *output);

#endif
