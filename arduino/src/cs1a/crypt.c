#include "cs1a.h"

unsigned char *crypt_rand(unsigned char *s, int len)
{
  unsigned char *x = s;
//  DEBUG_PRINTF("RAND %lu %d",s,len);
  while(len-- > 0)
  {
    *x = (unsigned char)random();
    x++;
  }
  return s;
}

unsigned char *crypt_hash(unsigned char *input, unsigned long len, unsigned char *output)
{
  sha256((uint8_t (*)[32])output,input,len*8);
  return output;
}

char *crypt_err()
{
  return 0;
}

