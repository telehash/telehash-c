#include "crypt_libtom.h"

unsigned char *crypt_rand(unsigned char *s, int len)
{
  yarrow_read(s,len,&_crypt_libtom_prng);
  return s;
}

// these are used by crypt_libtom_*.c

char *crypt_libtom_err()
{
  return (char*)error_to_string(_crypt_libtom_err);
}

int crypt_libtom_init()
{
  if(_crypt_libtom_inited) return 0;
  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);
  if ((_crypt_libtom_err = rng_make_prng(128, find_prng("yarrow"), &_crypt_libtom_prng, NULL)) != CRYPT_OK) return -1;
  return 0;
}

