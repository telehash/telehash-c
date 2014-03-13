#include "crypt_libtom.h"

unsigned char *crypt_rand(unsigned char *s, int len)
{
  yarrow_read(s,len,&_crypt_libtom_prng);
  return s;
}

unsigned char *crypt_hash(unsigned char *input, unsigned long len, unsigned char *output)
{
  unsigned long hlen = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"), input, len, output, &hlen)) != CRYPT_OK) return 0;
  return output;
}

char *crypt_err()
{
  return (char*)error_to_string(_crypt_libtom_err);
}

// used by crypt_libtom_*.c

int crypt_libtom_init()
{
  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);
  if ((_crypt_libtom_err = rng_make_prng(128, find_prng("yarrow"), &_crypt_libtom_prng, NULL)) != CRYPT_OK) return -1;
  return 0;
}

