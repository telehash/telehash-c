#ifndef crypt_h
#define crypt_h

#include <tomcrypt.h>
#include <tommath.h>
#include "util.h"

int crypt_init()
{
  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);
  return CRYPT_OK;
}

int crypt_hashname(unsigned char *key, int klen, unsigned char *hn)
{
  unsigned char bin[32];
  unsigned long len=32;
  int err;
  if ((err = hash_memory(find_hash("sha256"),key,klen,bin,&len)) != CRYPT_OK) return err;
  util_hex(bin,len,hn);
  return CRYPT_OK;
}

#endif