#include "cs1a.h"

void aes_128_ctr(unsigned char *key, size_t length, unsigned char iv[16], const unsigned char *input, unsigned char *output)
{
  mbedtls_aes_context ctx;
  size_t off = 0;
  unsigned char block[16];

  mbedtls_aes_setkey_enc(&ctx,key,128);
  mbedtls_aes_crypt_ctr(&ctx,length,&off,iv,block,input,output);
}

