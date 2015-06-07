#include "aes_i.h"

void aes_128_ctr(unsigned char *key, size_t length, unsigned char iv[16], const unsigned char *input, unsigned char *output)
{
  if(input!=output)
      memcpy(output,input,length);
  aes_128_ctr_encrypt(key,iv,output,length);
}

