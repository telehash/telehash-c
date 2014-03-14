#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "sha256.h"

unsigned char *crypt_rand(unsigned char *s, int len)
{

 unsigned char *x = s;
 
  while(len-- > 0)
  {
    *x = (unsigned char)random();
    x++;
  }
  return s;
}

unsigned char *crypt_hash(unsigned char *input, unsigned long len, unsigned char *output)
{
  sha256(input,len,output,0);
  return output;
}

char *crypt_err()
{
  return 0;
}

