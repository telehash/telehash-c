#include <stddef.h>
#include "hmac-sha256.h"

void hmac_256(const unsigned char *key, size_t keylen, const unsigned char *input, size_t ilen, unsigned char output[32])
{
  hmac_sha256(output, key, keylen*8, input, ilen*8);
}
