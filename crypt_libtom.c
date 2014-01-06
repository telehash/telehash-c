#include <tomcrypt.h>
#include <tommath.h>
#include "util.h"
#include "crypt.h"

struct crypt_struct
{
  rsa_key rsa;
  unsigned char hashname[32];
};

// simple way to track last known error from libtomcrypt and return it for debugging
int _crypt_err = 0;
char *crypt_err()
{
  return (char*)error_to_string(_crypt_err);
}

int crypt_init()
{
  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);
  return 0;
}

crypt_t crypt_new(unsigned char *key, int len)
{
  unsigned long der_len = 2048, hnlen=32;
  unsigned char der[der_len];
  
  if(!key || !len || len > der_len) return NULL;

  crypt_t c = malloc(sizeof (struct crypt_struct));
  bzero(c, sizeof (struct crypt_struct));
  
  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, &der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  // try to load rsa public key
  if((_crypt_err = rsa_import(der, der_len, &(c->rsa))) != CRYPT_OK) {
    free(c);
    return NULL;
  }

  // re-export it to be safe
  der_len = 2048;
  if((_crypt_err = rsa_export(der, &der_len, PK_PUBLIC, &(c->rsa))) != CRYPT_OK) {
    free(c);
    return NULL;
  }
  
  // generate hashname
  if((_crypt_err = hash_memory(find_hash("sha256"),der,der_len,c->hashname,&hnlen)) != CRYPT_OK){
    free(c);
    return NULL;
  }

  return c;
}

void crypt_free(crypt_t c)
{
  free(c);
}

unsigned char *crypt_hashname(crypt_t c)
{
  return c->hashname;
}

int crypt_private(crypt_t c, unsigned char *key, int len)
{
  unsigned long der_len = 4096;
  unsigned char der[der_len];
  
  if(!c || !key || !len || len > der_len) return 1;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, &der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  if((_crypt_err = rsa_import(der, der_len, &(c->rsa))) != CRYPT_OK) return 1;

  return 0;
}
