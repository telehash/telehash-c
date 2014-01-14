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

packet_t crypt_lineize(crypt_t c, packet_t p)
{
  packet_t line;
  if(!c || !p || !c->lined) return NULL;
  line = packet_chain(p);
  return line;
}

// create a new open packet
packet_t crypt_openize(crypt_t c)
{
  unsigned char key[32], iv[16], hex[33], hn[65], pub[65], *enc;
  packet_t open, inner;
  unsigned long len;
  symmetric_CTR ctr;

  if(!c) return NULL;
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
  packet_set_str(inner,"line",(char*)util_hex(c->lineOut,32,hex));
  packet_set_str(inner,"to",(char*)util_hex(c->hashname,32,hn));
  packet_set_int(inner,"at",c->at);

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

  /*
  // encrypt the ecc key
  len2 = sizeof(ecc_enc);
  if ((err = rsa_encrypt_key(self_eccpub, 65, ecc_enc, &len2, 0, 0, &prng, find_prng("yarrow"), find_hash("sha1"), &seed_rsa)) != CRYPT_OK) {
    printf("rsa_encrypt error: %s\n", error_to_string(err));
    return -1;
  }
  len = sizeof(open);
  if ((err = base64_encode(ecc_enc, len2, (unsigned char*)open, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  open[len] = 0;

  // sign the inner_enc
  len = 32;
  if ((err = hash_memory(find_hash("sha256"),inner_enc,inner_len,sighash,&len)) != CRYPT_OK) {
     printf("Error hashing key: %s\n", error_to_string(err));
     return -1;
  }
  len = sizeof(inner_sig);
  if ((err = rsa_sign_hash_ex(sighash, 32, inner_sig, &len, LTC_PKCS_1_V1_5, &prng, find_prng("yarrow"), find_hash("sha256"), 12, &self_rsa)) != CRYPT_OK) {
    printf("rsa_sign error: %s\n", error_to_string(err));
    return -1;
  }
	// encrypt the signature, create the new aes key+cipher first
  memcpy(csig,self_eccpub,65);
  memcpy(csig+65,rand16,16);
  len2 = 32;
  if ((err = hash_memory(find_hash("sha256"),csig,65+16,aeskey,&len2)) != CRYPT_OK) {
    printf("Error hashing key: %s\n", error_to_string(err));
    return -1;
	}
  if ((err = ctr_start(find_cipher("aes"),aesiv,aeskey,32,0,CTR_COUNTER_BIG_ENDIAN,&ctr)) != CRYPT_OK) {
    printf("ctr_start error: %s\n",error_to_string(err));
    return -1;
  }
  if ((err = ctr_encrypt(inner_sig,inner_csig,len,&ctr)) != CRYPT_OK) {
     printf("ctr_encrypt error: %s\n", error_to_string(err));
     return -1;
  }
  len2 = sizeof(sig);
  if ((err = base64_encode(inner_csig, len, (unsigned char*)sig, &len2)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  sig[len2] = 0;
  
  // create the outer open packet
  iat = (char*)outer+2;
  sprintf(iat, "{\"type\":\"open\",\"open\":\"%s\",\"iv\":\"%s\",\"sig\":\"%s\"}",open,iv,sig);
  printf("OUTER: %s\n",iat);
  iatlen = htons(strlen(iat));
  memcpy(outer,&iatlen,2);
  outer_len = 2+strlen(iat);
  memcpy(outer+outer_len,inner_enc,inner_len);
  outer_len += inner_len;
  printf("outer packet len %d: %s\n",outer_len,hex(outer,outer_len,out));  
*/
  
  return open;
}
