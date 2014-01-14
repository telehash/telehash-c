#include <tomcrypt.h>
#include <tommath.h>
#include "util.h"
#include "crypt.h"

struct crypt_struct
{
  rsa_key rsa;
  ecc_key ecc;
  unsigned char hashname[32];
  int private, lined, at;
  unsigned char lineOut[16], lineIn[16];
};

// simple way to track last known error from libtomcrypt and return it for debugging
int _crypt_err = 0;
char *crypt_err()
{
  return (char*)error_to_string(_crypt_err);
}

prng_state _crypt_prng;
int crypt_init()
{
  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);
  if ((_crypt_err = rng_make_prng(128, find_prng("yarrow"), &_crypt_prng, NULL)) != CRYPT_OK) return -1;
  return 0;
}

crypt_t crypt_new(unsigned char *key, int len)
{
  int der_len = 512;
  unsigned long hnlen=32;
  unsigned char der[der_len];
  
  if(!key || !len || len > der_len) return NULL;

  crypt_t c = malloc(sizeof (struct crypt_struct));
  bzero(c, sizeof (struct crypt_struct));
  
  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, (unsigned long*)&der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  // try to load rsa public key
  if((_crypt_err = rsa_import(der, der_len, &(c->rsa))) != CRYPT_OK) {
    free(c);
    return NULL;
  }

  // re-export it to be safe
  der_len = crypt_der(c, der, 2048);
  if(!der_len) {
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

int crypt_der(crypt_t c, unsigned char *der, int len)
{
  if(!c || !der) return 0;
  if((_crypt_err = rsa_export(der, (unsigned long*)&len, PK_PUBLIC, &(c->rsa))) != CRYPT_OK) return 0;
  return len;
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
  
  if(!c) return 1;
  if(c->private) return 0; // already loaded

  if(!key || !len || len > der_len) return 1;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, &der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  if((_crypt_err = rsa_import(der, der_len, &(c->rsa))) != CRYPT_OK) return 1;
  c->private = 1;
  return 0;
}

unsigned char *crypt_rand(unsigned char *s, int len)
{
  yarrow_read(s,len,&_crypt_prng);
  return s;
}

packet_t crypt_lineize(crypt_t c, crypt_t self, packet_t p)
{
  packet_t line;
  if(!c || !p || !c->lined) return NULL;
  line = packet_chain(p);
  return line;
}

packet_t crypt_delineize(crypt_t c, crypt_t self, packet_t p)
{
  if(!c || !self || !p || !c->lined) return NULL;
  return NULL;
}

// create a new open packet
packet_t crypt_openize(crypt_t c, crypt_t self)
{
  unsigned char key[32], iv[16], hex[33], hn[65], sig[256], esig[256], pub[65], epub[256], b64[512], *enc;
  packet_t open, inner;
  unsigned long len, len2;
  symmetric_CTR ctr;

  if(!c || !self) return NULL;
  hex[32] = hn[64] = 0;

  // create transient crypto stuff
  if(!c->at)
  {
    if((_crypt_err = ecc_make_key(&_crypt_prng, find_prng("yarrow"), 32, &(c->ecc))) != CRYPT_OK) return NULL;
    crypt_rand(c->lineOut,16);
    c->at = (int)time(0);
  }

  // create the inner/open packet
  inner = packet_new();
  packet_set_str(inner,"line",(char*)util_hex(c->lineOut,16,hex));
  packet_set_str(inner,"to",(char*)util_hex(c->hashname,32,hn));
  packet_set_int(inner,"at",c->at);
  len = crypt_der(self, b64, 512);
  packet_body(inner,b64,len);

  open = packet_chain(inner);
  packet_set_str(open,"type","open");

  // create the aes iv and key from ecc
  crypt_rand(iv, 16);
  packet_set_str(open,"iv", (char*)util_hex(iv,16,hex));
  len = sizeof(pub);
  if((_crypt_err = ecc_ansi_x963_export(&(c->ecc), pub, &len)) != CRYPT_OK) return packet_free(open);
  len = sizeof(key);
  if((_crypt_err = hash_memory(find_hash("sha256"), pub, 65, key, &len)) != CRYPT_OK) return packet_free(open);

  // create aes cipher now and encrypt the inner
  if((_crypt_err = ctr_start(find_cipher("aes"), iv, key, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(open);
  enc = malloc(packet_len(inner));
  if((_crypt_err = ctr_encrypt(packet_raw(inner),enc,packet_len(inner),&ctr)) != CRYPT_OK)
  {
    free(enc);
    return packet_free(open);
  }
  packet_body(open,enc,packet_len(inner));
  free(enc);

  // encrypt the ecc key and attach it
  len = sizeof(epub);
  if((_crypt_err = rsa_encrypt_key(pub, 65, epub, &len, 0, 0, &_crypt_prng, find_prng("yarrow"), find_hash("sha1"), &(c->rsa))) != CRYPT_OK) return packet_free(open);
  len2 = sizeof(b64);
  if((_crypt_err = base64_encode(epub, len, b64, &len2)) != CRYPT_OK) return packet_free(open);
  b64[len2] = 0;
  packet_set_str(open, "open", (char*)b64);

  // sign the inner packet
  len = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"),open->body,open->body_len,key,&len)) != CRYPT_OK) return packet_free(open);
  len = sizeof(sig);
  if((_crypt_err = rsa_sign_hash_ex(key, 32, sig, &len, LTC_PKCS_1_V1_5, &_crypt_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(self->rsa))) != CRYPT_OK) return packet_free(open);

	// encrypt the signature, create the new aes key+cipher first
  memcpy(b64,pub,65);
  memcpy(b64+65,c->lineOut,16);
  len2 = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"), b64, 65+16, key, &len2)) != CRYPT_OK) return packet_free(open);
  if((_crypt_err = ctr_start(find_cipher("aes"), iv, key, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(open);
  if((_crypt_err = ctr_encrypt(sig, esig, len, &ctr)) != CRYPT_OK) return packet_free(open);
  len2 = sizeof(b64);
  if((_crypt_err = base64_encode(esig, len, b64, &len2)) != CRYPT_OK) return packet_free(open);
  b64[len2] = 0;
  packet_set_str(open, "sig", (char*)b64);
  
  return open;
}
