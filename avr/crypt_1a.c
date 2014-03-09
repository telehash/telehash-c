#include "switch.h"
#include "./ecc.h"
#include "./aes.h"
#include "./sha256.h"
#include "./sha1.h"
#include "./hmac.h"
#include "./base64_enc.h"
#include "./base64_dec.h"

typedef struct crypt_1a_struct
{
  uint8_t id_secret[ECC_BYTES], id_public[ECC_BYTES *2], line_secret[ECC_BYTES], line_public[ECC_BYTES *2];
  uint16_t seq;
} *crypt_1a_t;

int RNG(uint8_t *p_dest, unsigned p_size)
{
  while(p_size--)
  {
    *p_dest = (uint8_t)random();
    p_dest++;
  }
  return 1;
}

int crypt_init_1a()
{
  ecc_set_rng(&RNG);
}

int crypt_new_1a(crypt_t c, unsigned char *key, int len)
{
  unsigned char hash[SHA1_HASH_BYTES];
  crypt_1a_t cs;
  
  if(!key || len <= 0) return 1;
  
  c->cs = malloc(sizeof(struct crypt_1a_struct));
  memset(c->cs, 0, sizeof (struct crypt_1a_struct));
  cs = (crypt_1a_t)c->cs;

  if(len == ECC_BYTES*2)
  {
    memcpy(cs->id_public,key,ECC_BYTES*2);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength(key,0) != ECC_BYTES*2 || base64dec(cs->id_public,key,0)) return -1;
  }
  
  // generate fingerprint
  sha1(hash,cs->id_public,ECC_BYTES*2);

  // create line ephemeral key
  ecc_make_key(cs->line_public, cs->line_private);

  // alloc/copy in the public values (free'd by crypt_free)  
  c->part = malloc(SHA1_HASH_BYTES*2+1);
  c->keylen = ECC_BYTES*2;
  c->key = malloc(c->keylen);
  memcpy(c->key,cs->id_public,ECC_BYTES*2);
  util_hex(hash,SHA1_HASH_BYTES,(unsigned char*)c->part);

  return 0;
}


void crypt_free_1a(crypt_t c)
{
  if(c->cs) free(c->cs);
}

int crypt_keygen_1a(packet_t p)
{
  unsigned long len, len2;
  unsigned char *buf, *b64;
  rsa_key rsa;

  if ((_crypt_1a_err = rsa_make_key(&_crypt_1a_prng, find_prng("yarrow"), 256, 65537, &rsa)) != CRYPT_OK) return -1;

  // this determines the size needed into len;
  len = 1;
  buf = malloc(len);
  rsa_export(buf, &len, PK_PRIVATE, &rsa);
  buf = realloc(buf,len);
  len2 = len*1.4;
  b64 = malloc(len2);

  // export/base64 private
  if((_crypt_1a_err = rsa_export(buf, &len, PK_PRIVATE, &rsa)) != CRYPT_OK)
  {
    free(buf);
    free(b64);
    return -1;
  }
  if((_crypt_1a_err = base64_encode(buf, len, b64, &len2)) != CRYPT_OK) {
    free(buf);
    free(b64);
    return -1;
  }
  b64[len2] = 0;
  packet_set_str(p,"1a_secret",(char*)b64);

  if((_crypt_1a_err = rsa_export(buf, &len, PK_PUBLIC, &rsa)) != CRYPT_OK)
  {
    free(buf);
    free(b64);
    return -1;
  }
  if((_crypt_1a_err = base64_encode(buf, len, b64, &len2)) != CRYPT_OK) {
    free(buf);
    free(b64);
    return -1;
  }
  b64[len2] = 0;
  packet_set_str(p,"1a",(char*)b64);
  return 0;
}

int crypt_private_1a(crypt_t c, unsigned char *key, int len)
{
  unsigned long der_len = 4096;
  unsigned char der[der_len];
  crypt_1a_t cs = (crypt_1a_t)c->cs;
  
  if(len > der_len) return 1;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, &der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  if((_crypt_1a_err = rsa_import(der, der_len, &(cs->rsa))) != CRYPT_OK) return 1;
  c->isprivate = 1;
  return 0;
}

packet_t crypt_lineize_1a(crypt_t c, packet_t p)
{
  packet_t line;
  unsigned long len;
  crypt_1a_t cs = (crypt_1a_t)c->cs;

  line = packet_chain(p);
  packet_body(line,NULL,packet_len(p)+16+16+16);
  memcpy(line->body,c->lineIn,16);
  crypt_rand(line->body+16,16);

  if((_crypt_1a_err = gcm_add_iv(&(cs->gcmOut), line->body+16, 16)) != CRYPT_OK) return packet_free(line);
  gcm_add_aad(&(cs->gcmOut),NULL,0);
  if((_crypt_1a_err = gcm_process(&(cs->gcmOut),packet_raw(p),packet_len(p),line->body+16+16,GCM_ENCRYPT)) != CRYPT_OK) return packet_free(line);
  len = 16;
  if((_crypt_1a_err = gcm_done(&(cs->gcmOut),line->body+16+16+packet_len(p), &len)) != CRYPT_OK) return packet_free(line);

  return line;
}

packet_t crypt_delineize_1a(crypt_t c, packet_t p)
{
  packet_t line;
  unsigned long len;
  crypt_1a_t cs = (crypt_1a_t)c->cs;

  if((_crypt_1a_err = gcm_add_iv(&(cs->gcmIn), p->body+16, 16)) != CRYPT_OK) return packet_free(p);
  gcm_add_aad(&(cs->gcmIn),NULL,0);
  if((_crypt_1a_err = gcm_process(&(cs->gcmIn),p->body+16+16,p->body_len-(16+16+16),p->body+16+16,GCM_DECRYPT)) != CRYPT_OK) return packet_free(p);
  len = 16;
  if((_crypt_1a_err = gcm_done(&(cs->gcmIn),p->body+(p->body_len-16), &len)) != CRYPT_OK) return packet_free(p);

  line = packet_parse(p->body+16+16, p->body_len-(16+16+16));
  packet_free(p);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_1a(crypt_t c, packet_t inner)
{
  unsigned char ecc[65], secret[32], input[64];
  char *hecc;
  unsigned long len;
  crypt_1a_t cs;
  ecc_key eccIn;
  
  cs = (crypt_1a_t)c->cs;
  hecc = packet_get_str(inner,"ecc"); // it's where we stashed it
  if(!hecc || strlen(hecc) != 128) return 1;

  // do the diffie hellman
  ecc[0] = 0x04; // make it the "uncompressed" format
  util_unhex((unsigned char*)hecc,128,ecc+1);
  if((_crypt_1a_err = ecc_ansi_x963_import(ecc, 65, &eccIn)) != CRYPT_OK) return 1;
  len = sizeof(secret);
  if((_crypt_1a_err = ecc_shared_secret(&(cs->eccOut),&eccIn,secret,&len)) != CRYPT_OK) return 1;

  // make line keys!
  memcpy(input,secret,32);
  memcpy(input+32,c->lineOut,16);
  memcpy(input+48,c->lineIn,16);
  len = 32;
  if((_crypt_1a_err = hash_memory(find_hash("sha256"), input, 64, secret, &len)) != CRYPT_OK) return 1;
  if((_crypt_1a_err = gcm_init(&(cs->gcmOut), find_cipher("aes"), secret, 32)) != CRYPT_OK) return 1;

  memcpy(input+32,c->lineIn,16);
  memcpy(input+48,c->lineOut,16);
  len = 32;
  if((_crypt_1a_err = hash_memory(find_hash("sha256"), input, 64, secret, &len)) != CRYPT_OK) return 1;
  if((_crypt_1a_err = gcm_init(&(cs->gcmIn), find_cipher("aes"), secret, 32)) != CRYPT_OK) return 1;

  return 0;
}

// create a new open packet
packet_t crypt_openize_1a(crypt_t self, crypt_t c, packet_t inner)
{
  unsigned char key[32], iv[16], sig[256], buf[260], upub[65], *pub;
  packet_t open;
  unsigned long len, inner_len;
  gcm_state gcm;
  crypt_1a_t cs = (crypt_1a_t)c->cs, scs = (crypt_1a_t)self->cs;

  open = packet_chain(inner);
  packet_json(open,&(self->csid),1);
  inner_len = packet_len(inner);
  packet_body(open,NULL,256+260+inner_len+16);

  // create the aes iv and key from ecc
  util_unhex((unsigned char*)"00000000000000000000000000000001",32,iv);
  len = sizeof(upub);
  if((_crypt_1a_err = ecc_ansi_x963_export(&(cs->eccOut), upub, &len)) != CRYPT_OK) return packet_free(open);
  pub = upub+1; // take off the 0x04 prefix
  len = sizeof(key);
  if((_crypt_1a_err = hash_memory(find_hash("sha256"), pub, 64, key, &len)) != CRYPT_OK) return packet_free(open);

  // create aes cipher now and encrypt the inner
  len = 16;
//  if((_crypt_1a_err = gcm_memory(find_cipher("aes"), key, 32, iv, 16, NULL, 0, packet_raw(inner), inner_len, open->body+256+260, open->body+256+260+inner_len, &len, GCM_ENCRYPT)) != CRYPT_OK) return packet_free(open);
  if((_crypt_1a_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return packet_free(open);
  if((_crypt_1a_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return packet_free(open);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_1a_err = gcm_process(&gcm,packet_raw(inner),inner_len,open->body+256+260,GCM_ENCRYPT)) != CRYPT_OK) return packet_free(open);
  if((_crypt_1a_err = gcm_done(&gcm, open->body+256+260+inner_len, &len)) != CRYPT_OK) return packet_free(open);

  // encrypt the ecc key and attach it
  len = sizeof(buf);
  if((_crypt_1a_err = rsa_encrypt_key(pub, 64, open->body, &len, 0, 0, &_crypt_1a_prng, find_prng("yarrow"), find_hash("sha1"), &(cs->rsa))) != CRYPT_OK) return packet_free(open);

  // sign the inner packet
  len = 32;
  if((_crypt_1a_err = hash_memory(find_hash("sha256"),open->body+256+260,inner_len+16,key,&len)) != CRYPT_OK) return packet_free(open);
  len = sizeof(sig);
  if((_crypt_1a_err = rsa_sign_hash_ex(key, 32, sig, &len, LTC_PKCS_1_V1_5, &_crypt_1a_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(scs->rsa))) != CRYPT_OK) return packet_free(open);

	// encrypt the signature, create the new aes key+cipher first
  memcpy(buf,pub,64);
  memcpy(buf+64,c->lineOut,16);
  len = 32;
  if((_crypt_1a_err = hash_memory(find_hash("sha256"),buf,64+16,key,&len)) != CRYPT_OK) return packet_free(open);
  len = 4;
  if((_crypt_1a_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return packet_free(open);
  if((_crypt_1a_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return packet_free(open);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_1a_err = gcm_process(&gcm,sig,256,open->body+256,GCM_ENCRYPT)) != CRYPT_OK) return packet_free(open);
  if((_crypt_1a_err = gcm_done(&gcm, open->body+256+256, &len)) != CRYPT_OK) return packet_free(open);

  return open;
}

packet_t crypt_deopenize_1a(crypt_t self, packet_t open)
{
  unsigned char key[32], iv[16], buf[260], upub[65], *hline;
  unsigned long len, len2;
  int res;
  gcm_state gcm;
  rsa_key rsa;
  packet_t inner, tmp;
  crypt_1a_t cs = (crypt_1a_t)self->cs;

  if(open->body_len <= (256+260)) return NULL;
  inner = packet_new();
  packet_body(inner,NULL,open->body_len-(256+260+16));

  // decrypt the open
  len = sizeof(upub);
  if((_crypt_1a_err = rsa_decrypt_key(open->body, 256, upub, &len, 0, 0, find_hash("sha1"), &res, &(cs->rsa))) != CRYPT_OK || len != 64) return packet_free(inner);

  // create the aes key/iv to decipher the body
  util_unhex((unsigned char*)"00000000000000000000000000000001",32,iv);
  len = 32;
  if((_crypt_1a_err = hash_memory(find_hash("sha256"),upub,64,key,&len)) != CRYPT_OK) return packet_free(inner);

  // decrypt
  if((_crypt_1a_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return packet_free(inner);
  if((_crypt_1a_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return packet_free(inner);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_1a_err = gcm_process(&gcm,inner->body,inner->body_len,open->body+256+260,GCM_DECRYPT)) != CRYPT_OK) return packet_free(inner);
  len = 16;
  if((_crypt_1a_err = gcm_done(&gcm, open->body+256+260+inner->body_len, &len)) != CRYPT_OK) return packet_free(inner);
  if((tmp = packet_parse(inner->body,inner->body_len)) == NULL) return packet_free(inner);
  packet_free(inner);
  inner = tmp;

  // decipher/verify the signature
  if((_crypt_1a_err = rsa_import(inner->body, inner->body_len, &rsa)) != CRYPT_OK) return packet_free(inner);
  hline = (unsigned char*)packet_get_str(inner,"line");
  if(!hline) return packet_free(inner);
  memcpy(buf,upub,64);
  util_unhex(hline,32,buf+64);
  len = 32;
  len2 = 4;
  if((_crypt_1a_err = hash_memory(find_hash("sha256"),buf,64+16,key,&len)) != CRYPT_OK
    || (_crypt_1a_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK
    || (_crypt_1a_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK
    || (_crypt_1a_err = gcm_add_aad(&gcm, NULL, 0)) != CRYPT_OK
    || (_crypt_1a_err = gcm_process(&gcm,open->body+256,256,open->body+256,GCM_DECRYPT)) != CRYPT_OK
    || (_crypt_1a_err = gcm_done(&gcm, open->body+256+256, &len2)) != CRYPT_OK
    || (_crypt_1a_err = hash_memory(find_hash("sha256"),open->body+256+260,open->body_len-(256+260),key,&len)) != CRYPT_OK
    || (_crypt_1a_err = rsa_verify_hash_ex(open->body+256, 256, key, len, LTC_PKCS_1_V1_5, find_hash("sha256"), 0, &res, &rsa)) != CRYPT_OK
    || res != 1) return packet_free(inner);

  // stash the hex line key w/ the inner
  util_hex(upub,64,buf);
  packet_set_str(inner,"ecc",(char*)buf);

  return inner;
}

