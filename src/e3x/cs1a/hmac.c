#include "sha256.h"

void hmac_256(const unsigned char *key, size_t keylen, const unsigned char *input, size_t ilen, unsigned char output[32])
{
  hmac_sha256(key, keylen, input, ilen, output);
}

void sha256( const unsigned char *input, size_t ilen,
             unsigned char output[32], int is224 )
{
    const u8 *inn[1];
    inn[0]=input;
    size_t s[1];
    s[0]=ilen;
    sha256_vector(1, inn, s,output);
}
