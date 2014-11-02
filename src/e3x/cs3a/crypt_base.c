#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sodium.h>

unsigned char *crypt_rand(unsigned char *s, int len)
{
  randombytes_buf((void * const)s, (const size_t)len);
  return s;
}

unsigned char *crypt_hash(unsigned char *input, unsigned long len, unsigned char *output)
{
  crypto_hash_sha256(output,input,(unsigned long)len);
  return output;
}

char *crypt_err()
{
  return 0;
}

