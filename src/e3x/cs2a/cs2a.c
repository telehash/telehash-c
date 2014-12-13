#include <tomcrypt.h>
#include <tommath.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "sha256.h"
#include "e3x.h"
#include "e3x/cipher3.h"
#include "util_sys.h"

// undefine the void* aliases so we can define them locally
#undef local_t
#undef remote_t
#undef ephemeral_t

typedef struct local_struct
{
  uint8_t secret[uECC_BYTES], key[uECC_BYTES *2];
} *local_t;

typedef struct remote_struct
{
  uint8_t key[uECC_BYTES *2];
  uint8_t esecret[uECC_BYTES], ekey[uECC_BYTES *2], ecomp[uECC_BYTES+1];
  uint32_t seq;
} *remote_t;

typedef struct ephemeral_struct
{
  uint8_t enckey[16], deckey[16], token[16];
  uint32_t seq;
} *ephemeral_t;

// these are all the locally implemented handlers defined in cipher3.h

static uint8_t *cipher_hash(uint8_t *input, uint32_t len, uint8_t *output);
static uint8_t *cipher_err(void);
static uint8_t cipher_generate(lob_t keys, lob_t secrets);

static local_t local_new(lob_t keys, lob_t secrets);
static void local_free(local_t local);
static lob_t local_decrypt(local_t local, lob_t outer);

static remote_t remote_new(lob_t key, uint8_t *token);
static void remote_free(remote_t remote);
static uint8_t remote_verify(remote_t remote, local_t local, lob_t outer);
static lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner);

static ephemeral_t ephemeral_new(remote_t remote, lob_t outer);
static void ephemeral_free(ephemeral_t ephemeral);
static lob_t ephemeral_encrypt(ephemeral_t ephemeral, lob_t inner);
static lob_t ephemeral_decrypt(ephemeral_t ephemeral, lob_t outer);


static int RNG(uint8_t *p_dest, unsigned p_size)
{
  e3x_rand(p_dest,p_size);
  return 1;
}

cipher3_t cs2a_init(lob_t options)
{
  cipher3_t ret = malloc(sizeof(struct cipher3_struct));
  if(!ret) return NULL;
  memset(ret,0,sizeof (struct cipher3_struct));
  
  // identifying markers
  ret->id = CS_2a;
  ret->csid = 0x2a;
  memcpy(ret->hex,"2a",3);

  // TODO

  return ret;
}

uint8_t *cipher_hash(uint8_t *input, uint32_t len, uint8_t *output)
{
  sha256(input,len,output,0);
  return output;
}

uint8_t *cipher_err(void)
{
  return 0;
}

uint8_t cipher_generate(lob_t keys, lob_t secrets)
{
  uint8_t secret[uECC_BYTES], key[uECC_BYTES*2], comp[uECC_BYTES+1];

  if(!uECC_make_key(key, secret)) return 1;
  uECC_compress(key,comp);
  lob_set_base32(keys,"1a",comp,uECC_BYTES+1);
  lob_set_base32(secrets,"1a",secret,uECC_BYTES);

  return 0;
}

static void fold1(uint8_t in[32], uint8_t out[16])
{
  uint8_t i;
  for(i=0;i<16;i++) out[i] = in[i] ^ in[i+16];
}

static void fold3(uint8_t in[32], uint8_t out[4])
{
  uint8_t i, buf[16];
  for(i=0;i<16;i++) buf[i] = in[i] ^ in[i+16];
  for(i=0;i<8;i++) buf[i] ^= buf[i+8];
  for(i=0;i<4;i++) out[i] = buf[i] ^ buf[i+4];
}

local_t local_new(lob_t keys, lob_t secrets)
{
  local_t local;
  lob_t key, secret;

  if(!keys) keys = lob_linked(secrets); // for convenience
  key = lob_get_base32(keys,"1a");
  if(!key || key->body_len != uECC_BYTES+1) return LOG("invalid key %d != %d",(key)?key->body_len:0,uECC_BYTES+1);

  secret = lob_get_base32(secrets,"1a");
  if(!secret || secret->body_len != uECC_BYTES) return LOG("invalid secret len %d",(secret)?secret->body_len:0);
  
  if(!(local = malloc(sizeof(struct local_struct)))) return NULL;
  memset(local,0,sizeof (struct local_struct));

  // copy in key/secret data
  uECC_decompress(key->body,local->key);
  memcpy(local->secret,secret->body,secret->body_len);
  lob_free(key);
  lob_free(secret);

  return local;
}

void local_free(local_t local)
{
  free(local);
  return;
}

lob_t local_decrypt(local_t local, lob_t outer)
{
  uint8_t key[uECC_BYTES*2], shared[uECC_BYTES], iv[16], hash[32];
  lob_t inner, tmp;

//  * `KEY` - 21 bytes, the sender's ephemeral exchange public key in compressed format
//  * `IV` - 4 bytes, a random but unique value determined by the sender
//  * `INNER` - (minimum 21+2 bytes) the AES-128-CTR encrypted inner packet ciphertext
//  * `HMAC` - 4 bytes, the calculated HMAC of all of the previous KEY+INNER bytes

  if(outer->body_len <= (21+4+0+4)) return NULL;
  tmp = lob_new();
  if(!lob_body(tmp,NULL,outer->body_len-(4+21+4))) return lob_free(tmp);

  // get the shared secret to create the iv+key for the open aes
  uECC_decompress(outer->body,key);
  if(!uECC_shared_secret(key, local->secret, shared)) return lob_free(tmp);
  e3x_hash(shared,uECC_BYTES,hash);
  fold1(hash,hash);
  memset(iv,0,16);
  memcpy(iv,outer->body+21,4);

  // decrypt the inner
  aes_128_ctr(hash,tmp->body_len,iv,outer->body+4+21,tmp->body);

  // load inner packet
  inner = lob_parse(tmp->body,tmp->body_len);
  lob_free(tmp);
  return inner;
}

remote_t remote_new(lob_t key, uint8_t *token)
{
  uint8_t hash[32];
  remote_t remote;
  if(!key || key->body_len != uECC_BYTES+1) return LOG("invalid key %d != %d",(key)?key->body_len:0,uECC_BYTES+1);
  
  if(!(remote = malloc(sizeof(struct remote_struct)))) return NULL;
  memset(remote,0,sizeof (struct remote_struct));

  // copy in key and make ephemeral ones
  uECC_decompress(key->body,remote->key);
  uECC_make_key(remote->ekey, remote->esecret);
  uECC_compress(remote->ekey, remote->ecomp);
  if(token)
  {
    cipher_hash(remote->ecomp,16,hash);
    memcpy(token,hash,16);
  }

  // generate a random seq starting point for message IV's
  e3x_rand((uint8_t*)&(remote->seq),4);
  
  return remote;
}

void remote_free(remote_t remote)
{
  free(remote);
}

uint8_t remote_verify(remote_t remote, local_t local, lob_t outer)
{
  uint8_t shared[uECC_BYTES+4], hash[32];

  if(!remote || !local || !outer) return 1;
  if(outer->head_len != 1 || outer->head[0] != 0x1a) return 2;

  // generate the key for the hmac, combining the shared secret and IV
  if(!uECC_shared_secret(remote->key, local->secret, shared)) return 3;
  memcpy(shared+uECC_BYTES,outer->body+21,4);

  // verify
  hmac_256(shared,uECC_BYTES+4,outer->body,outer->body_len-4,hash);
  fold3(hash,hash);
  if(memcmp(hash,outer->body+(outer->body_len-4),4) != 0)
  {
    LOG("hmac failed");
    return 4;
  }

  return 0;
}

lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner)
{
  uint8_t shared[uECC_BYTES+4], iv[16], hash[32], csid = 0x1a;
  lob_t outer;
  uint32_t inner_len;

  outer = lob_new();
  lob_head(outer,&csid,1);
  inner_len = lob_len(inner);
  if(!lob_body(outer,NULL,21+4+inner_len+4)) return lob_free(outer);

  // copy in the ephemeral public key
  memcpy(outer->body, remote->ecomp, uECC_BYTES+1);

  // get the shared secret to create the iv+key for the open aes
  if(!uECC_shared_secret(remote->key, remote->esecret, shared)) return lob_free(outer);
  e3x_hash(shared,uECC_BYTES,hash);
  fold1(hash,hash);
  memset(iv,0,16);
  memcpy(iv,&(remote->seq),4);
  remote->seq++; // increment seq after every use
  memcpy(outer->body+21,iv,4); // send along the used IV

  // encrypt the inner into the outer
  aes_128_ctr(hash,inner_len,iv,lob_raw(inner),outer->body+21+4);

  // generate secret for hmac
  if(!uECC_shared_secret(remote->key, local->secret, shared)) return lob_free(outer);
  memcpy(shared+uECC_BYTES,outer->body+21,4); // use the IV too

  hmac_256(shared,uECC_BYTES+4,outer->body,21+4+inner_len,hash);
  fold3(hash,outer->body+21+4+inner_len); // write into last 4 bytes

  return outer;
}

ephemeral_t ephemeral_new(remote_t remote, lob_t outer)
{
  uint8_t ekey[uECC_BYTES*2], shared[uECC_BYTES+((uECC_BYTES+1)*2)], hash[32];
  ephemeral_t ephem;

  if(!remote) return NULL;
  if(!outer || outer->body_len < (uECC_BYTES+1)) return LOG("invalid outer");

  if(!(ephem = malloc(sizeof(struct ephemeral_struct)))) return NULL;
  memset(ephem,0,sizeof (struct ephemeral_struct));
  
  // create and copy in the exchange routing token
  e3x_hash(outer->body,16,hash);
  memcpy(ephem->token,hash,16);

  // generate a random seq starting point for channel IV's
  e3x_rand((uint8_t*)&(ephem->seq),4);

  // decompress the exchange key and get the shared secret
  uECC_decompress(outer->body,ekey);
  if(!uECC_shared_secret(ekey, remote->esecret, shared)) return LOG("ECDH failed");

  // combine inputs to create the digest
  memcpy(shared+uECC_BYTES,remote->ecomp,uECC_BYTES+1);
  memcpy(shared+uECC_BYTES+uECC_BYTES+1,outer->body,uECC_BYTES+1);
  e3x_hash(shared,uECC_BYTES+((uECC_BYTES+1)*2),hash);
  fold1(hash,ephem->enckey);

  memcpy(shared+uECC_BYTES,outer->body,uECC_BYTES+1);
  memcpy(shared+uECC_BYTES+uECC_BYTES+1,remote->ecomp,uECC_BYTES+1);
  e3x_hash(shared,uECC_BYTES+((uECC_BYTES+1)*2),hash);
  fold1(hash,ephem->deckey);

  return ephem;
}

void ephemeral_free(ephemeral_t ephem)
{
  free(ephem);
}

lob_t ephemeral_encrypt(ephemeral_t ephem, lob_t inner)
{
  lob_t outer;
  uint8_t iv[16], hmac[32];
  uint32_t inner_len;

  outer = lob_new();
  inner_len = lob_len(inner);
  if(!lob_body(outer,NULL,16+4+inner_len+4)) return lob_free(outer);

  // copy in token and create/copy iv
  memcpy(outer->body,ephem->token,16);
  memset(iv,0,16);
  memcpy(iv,&(ephem->seq),4);
  ephem->seq++;
  memcpy(outer->body+16,iv,4);

  // encrypt full inner into the outer
  aes_128_ctr(ephem->enckey,inner_len,iv,lob_raw(inner),outer->body+16+4);

  // generate mac key and mac the ciphertext
  memcpy(hmac,ephem->enckey,16);
  memcpy(hmac+16,iv,4);
  hmac_256(hmac,16+4,outer->body+16+4,inner_len,hmac);
  fold3(hmac,outer->body+16+4+inner_len);

  return outer;
}

lob_t ephemeral_decrypt(ephemeral_t ephem, lob_t outer)
{
  uint8_t iv[16], hmac[32];

  memset(iv,0,16);
  memcpy(iv,outer->body+16,4);

  memcpy(hmac,ephem->deckey,16);
  memcpy(hmac+16,iv,4);
  // mac just the ciphertext
  hmac_256(hmac,16+4,outer->body+16+4,outer->body_len-(4+16+4),hmac);
  fold3(hmac,hmac);

  if(memcmp(hmac,outer->body+(outer->body_len-4),4) != 0) return LOG("hmac failed");

  // decrypt in place
  aes_128_ctr(ephem->deckey,outer->body_len-(16+4+4),iv,outer->body+16+4,outer->body+16+4);

  // return parse attempt
  return lob_parse(outer->body+16+4, outer->body_len-(16+4+4));
}

/*

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


typedef struct crypt_libtom_struct
{
  rsa_key rsa;
  ecc_key eccOut;
  gcm_state gcmIn, gcmOut;
} *crypt_libtom_t;

int crypt_init_2a()
{
  return crypt_libtom_init();
}

int crypt_new_2a(crypt_t c, unsigned char *key, int len)
{
  int der_len = 512;
  unsigned long fplen=32;
  unsigned char der[der_len], fp[32];
  crypt_libtom_t cs;
  
  if(!key || !len || len > der_len) return 1;
  
  c->cs = malloc(sizeof(struct crypt_libtom_struct));
  memset(c->cs, 0, sizeof (struct crypt_libtom_struct));
  cs = (crypt_libtom_t)c->cs;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, (unsigned long*)&der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  // try to load rsa public key
  if((_crypt_libtom_err = rsa_import(der, der_len, &(cs->rsa))) != CRYPT_OK) return 1;

  // re-export it to be safe
  if((_crypt_libtom_err = rsa_export(der, (unsigned long*)&der_len, PK_PUBLIC, &(cs->rsa))) != CRYPT_OK) return 1;
  
  // generate fingerprint
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),der,der_len,fp,&fplen)) != CRYPT_OK) return 1;

  // create line ephemeral key
  if((_crypt_libtom_err = ecc_make_key(&_crypt_libtom_prng, find_prng("yarrow"), 32, &(cs->eccOut))) != CRYPT_OK) return 1;

  // alloc/copy in the public values (free'd by crypt_free)  
  c->part = malloc(65);
  c->keylen = der_len;
  c->key = malloc(c->keylen);
  memcpy(c->key,der,der_len);
  util_hex(fp,32,(unsigned char*)c->part);

  return 0;
}


void crypt_free_2a(crypt_t c)
{
  if(c->cs) free(c->cs);
}

int crypt_keygen_2a(lob_t p)
{
  unsigned long len, len2;
  unsigned char *buf, *b64;
  rsa_key rsa;

  if ((_crypt_libtom_err = rsa_make_key(&_crypt_libtom_prng, find_prng("yarrow"), 256, 65537, &rsa)) != CRYPT_OK) return -1;

  // this determines the size needed into len;
  len = 1;
  buf = malloc(len);
  rsa_export(buf, &len, PK_PRIVATE, &rsa);
  buf = realloc(buf,len);
  len2 = len*1.4;
  b64 = malloc(len2);

  // export/base64 private
  if((_crypt_libtom_err = rsa_export(buf, &len, PK_PRIVATE, &rsa)) != CRYPT_OK)
  {
    free(buf);
    free(b64);
    return -1;
  }
  if((_crypt_libtom_err = base64_encode(buf, len, b64, &len2)) != CRYPT_OK) {
    free(buf);
    free(b64);
    return -1;
  }
  b64[len2] = 0;
  lob_set_str(p,"2a_secret",(char*)b64);

  if((_crypt_libtom_err = rsa_export(buf, &len, PK_PUBLIC, &rsa)) != CRYPT_OK)
  {
    free(buf);
    free(b64);
    return -1;
  }
  if((_crypt_libtom_err = base64_encode(buf, len, b64, &len2)) != CRYPT_OK) {
    free(buf);
    free(b64);
    return -1;
  }
  b64[len2] = 0;
  lob_set_str(p,"2a",(char*)b64);
  return 0;
}

int crypt_private_2a(crypt_t c, unsigned char *key, int len)
{
  unsigned long der_len = 4096;
  unsigned char der[der_len];
  crypt_libtom_t cs = (crypt_libtom_t)c->cs;
  
  if(len > (int)der_len) return 1;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, &der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  if((_crypt_libtom_err = rsa_import(der, der_len, &(cs->rsa))) != CRYPT_OK) return 1;
  c->isprivate = 1;
  return 0;
}

lob_t crypt_lineize_2a(crypt_t c, lob_t p)
{
  lob_t line;
  unsigned long len;
  crypt_libtom_t cs = (crypt_libtom_t)c->cs;

  line = lob_chain(p);
  lob_body(line,NULL,lob_len(p)+16+16+16);
  memcpy(line->body,c->lineIn,16);
  crypt_rand(line->body+16,16);

  if((_crypt_libtom_err = gcm_add_iv(&(cs->gcmOut), line->body+16, 16)) != CRYPT_OK) return lob_free(line);
  gcm_add_aad(&(cs->gcmOut),NULL,0);
  if((_crypt_libtom_err = gcm_process(&(cs->gcmOut),lob_raw(p),lob_len(p),line->body+16+16,GCM_ENCRYPT)) != CRYPT_OK) return lob_free(line);
  len = 16;
  if((_crypt_libtom_err = gcm_done(&(cs->gcmOut),line->body+16+16+lob_len(p), &len)) != CRYPT_OK) return lob_free(line);

  return line;
}

lob_t crypt_delineize_2a(crypt_t c, lob_t p)
{
  lob_t line;
  unsigned long len;
  crypt_libtom_t cs = (crypt_libtom_t)c->cs;

  if((_crypt_libtom_err = gcm_reset(&(cs->gcmIn))) != CRYPT_OK) return lob_free(p);
  if((_crypt_libtom_err = gcm_add_iv(&(cs->gcmIn), p->body+16, 16)) != CRYPT_OK) return lob_free(p);
  gcm_add_aad(&(cs->gcmIn),NULL,0);
  if((_crypt_libtom_err = gcm_process(&(cs->gcmIn),p->body+16+16,p->body_len-(16+16+16),p->body+16+16,GCM_DECRYPT)) != CRYPT_OK) return lob_free(p);
  len = 16;
  if((_crypt_libtom_err = gcm_done(&(cs->gcmIn),p->body+(p->body_len-16), &len)) != CRYPT_OK) return lob_free(p);

  line = lob_parse(p->body+16+16, p->body_len-(16+16+16));
  lob_free(p);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_2a(crypt_t c, lob_t inner)
{
  unsigned char ecc[65], secret[32], input[64];
  char *hecc;
  unsigned long len;
  crypt_libtom_t cs;
  ecc_key eccIn;
  
  cs = (crypt_libtom_t)c->cs;
  hecc = lob_get_str(inner,"ecc"); // it's where we stashed it
  if(!hecc || strlen(hecc) != 128) return 1;

  // do the diffie hellman
  ecc[0] = 0x04; // make it the "uncompressed" format
  util_unhex((unsigned char*)hecc,128,ecc+1);
  if((_crypt_libtom_err = ecc_ansi_x963_import(ecc, 65, &eccIn)) != CRYPT_OK) return 1;
  len = sizeof(secret);
  if((_crypt_libtom_err = ecc_shared_secret(&(cs->eccOut),&eccIn,secret,&len)) != CRYPT_OK) return 1;

  // make line keys!
  memcpy(input,secret,32);
  memcpy(input+32,c->lineOut,16);
  memcpy(input+48,c->lineIn,16);
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"), input, 64, secret, &len)) != CRYPT_OK) return 1;
  if((_crypt_libtom_err = gcm_init(&(cs->gcmOut), find_cipher("aes"), secret, 32)) != CRYPT_OK) return 1;

  memcpy(input+32,c->lineIn,16);
  memcpy(input+48,c->lineOut,16);
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"), input, 64, secret, &len)) != CRYPT_OK) return 1;
  if((_crypt_libtom_err = gcm_init(&(cs->gcmIn), find_cipher("aes"), secret, 32)) != CRYPT_OK) return 1;

  return 0;
}

// create a new open packet
lob_t crypt_openize_2a(crypt_t self, crypt_t c, lob_t inner)
{
  unsigned char key[32], iv[16], sig[256], buf[260], upub[65], *pub;
  lob_t open;
  unsigned long len, inner_len;
  gcm_state gcm;
  crypt_libtom_t cs = (crypt_libtom_t)c->cs, scs = (crypt_libtom_t)self->cs;

  open = lob_chain(inner);
  lob_json(open,&(self->csid),1);
  inner_len = lob_len(inner);
  lob_body(open,NULL,256+260+inner_len+16);

  // create the aes iv and key from ecc
  util_unhex((unsigned char*)"00000000000000000000000000000001",32,iv);
  len = sizeof(upub);
  if((_crypt_libtom_err = ecc_ansi_x963_export(&(cs->eccOut), upub, &len)) != CRYPT_OK) return lob_free(open);
  pub = upub+1; // take off the 0x04 prefix
  len = sizeof(key);
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"), pub, 64, key, &len)) != CRYPT_OK) return lob_free(open);

  // create aes cipher now and encrypt the inner
  len = 16;
//  if((_crypt_libtom_err = gcm_memory(find_cipher("aes"), key, 32, iv, 16, NULL, 0, lob_raw(inner), inner_len, open->body+256+260, open->body+256+260+inner_len, &len, GCM_ENCRYPT)) != CRYPT_OK) return lob_free(open);
  if((_crypt_libtom_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return lob_free(open);
  if((_crypt_libtom_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return lob_free(open);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_libtom_err = gcm_process(&gcm,lob_raw(inner),inner_len,open->body+256+260,GCM_ENCRYPT)) != CRYPT_OK) return lob_free(open);
  if((_crypt_libtom_err = gcm_done(&gcm, open->body+256+260+inner_len, &len)) != CRYPT_OK) return lob_free(open);

  // encrypt the ecc key and attach it
  len = sizeof(buf);
  if((_crypt_libtom_err = rsa_encrypt_key(pub, 64, open->body, &len, 0, 0, &_crypt_libtom_prng, find_prng("yarrow"), find_hash("sha1"), &(cs->rsa))) != CRYPT_OK) return lob_free(open);

  // sign the inner packet
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),open->body+256+260,inner_len+16,key,&len)) != CRYPT_OK) return lob_free(open);
  len = sizeof(sig);
  if((_crypt_libtom_err = rsa_sign_hash_ex(key, 32, sig, &len, LTC_PKCS_1_V1_5, &_crypt_libtom_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(scs->rsa))) != CRYPT_OK) return lob_free(open);

	// encrypt the signature, create the new aes key+cipher first
  memcpy(buf,pub,64);
  memcpy(buf+64,c->lineOut,16);
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),buf,64+16,key,&len)) != CRYPT_OK) return lob_free(open);
  len = 4;
  if((_crypt_libtom_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return lob_free(open);
  if((_crypt_libtom_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return lob_free(open);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_libtom_err = gcm_process(&gcm,sig,256,open->body+256,GCM_ENCRYPT)) != CRYPT_OK) return lob_free(open);
  if((_crypt_libtom_err = gcm_done(&gcm, open->body+256+256, &len)) != CRYPT_OK) return lob_free(open);

  return open;
}

lob_t crypt_deopenize_2a(crypt_t self, lob_t open)
{
  unsigned char key[32], iv[16], buf[260], upub[65], *hline;
  unsigned long len, len2;
  int res;
  gcm_state gcm;
  rsa_key rsa;
  lob_t inner, tmp;
  crypt_libtom_t cs = (crypt_libtom_t)self->cs;

  if(open->body_len <= (256+260)) return NULL;
  inner = lob_new();
  lob_body(inner,NULL,open->body_len-(256+260+16));

  // decrypt the open
  len = sizeof(upub);
  if((_crypt_libtom_err = rsa_decrypt_key(open->body, 256, upub, &len, 0, 0, find_hash("sha1"), &res, &(cs->rsa))) != CRYPT_OK || len != 64) return lob_free(inner);

  // create the aes key/iv to decipher the body
  util_unhex((unsigned char*)"00000000000000000000000000000001",32,iv);
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),upub,64,key,&len)) != CRYPT_OK) return lob_free(inner);

  // decrypt
  if((_crypt_libtom_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return lob_free(inner);
  if((_crypt_libtom_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return lob_free(inner);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_libtom_err = gcm_process(&gcm,inner->body,inner->body_len,open->body+256+260,GCM_DECRYPT)) != CRYPT_OK) return lob_free(inner);
  len = 16;
  if((_crypt_libtom_err = gcm_done(&gcm, open->body+256+260+inner->body_len, &len)) != CRYPT_OK) return lob_free(inner);
  if((tmp = lob_parse(inner->body,inner->body_len)) == NULL) return lob_free(inner);
  lob_free(inner);
  inner = tmp;

  // decipher/verify the signature
  if((_crypt_libtom_err = rsa_import(inner->body, inner->body_len, &rsa)) != CRYPT_OK) return lob_free(inner);
  hline = (unsigned char*)lob_get_str(inner,"line");
  if(!hline) return lob_free(inner);
  memcpy(buf,upub,64);
  util_unhex(hline,32,buf+64);
  len = 32;
  len2 = 4;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),buf,64+16,key,&len)) != CRYPT_OK
    || (_crypt_libtom_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK
    || (_crypt_libtom_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK
    || (_crypt_libtom_err = gcm_add_aad(&gcm, NULL, 0)) != CRYPT_OK
    || (_crypt_libtom_err = gcm_process(&gcm,open->body+256,256,open->body+256,GCM_DECRYPT)) != CRYPT_OK
    || (_crypt_libtom_err = gcm_done(&gcm, open->body+256+256, &len2)) != CRYPT_OK
    || (_crypt_libtom_err = hash_memory(find_hash("sha256"),open->body+256+260,open->body_len-(256+260),key,&len)) != CRYPT_OK
    || (_crypt_libtom_err = rsa_verify_hash_ex(open->body+256, 256, key, len, LTC_PKCS_1_V1_5, find_hash("sha256"), 0, &res, &rsa)) != CRYPT_OK
    || res != 1) return lob_free(inner);

  // stash the hex line key w/ the inner
  util_hex(upub,64,buf);
  lob_set_str(inner,"ecc",(char*)buf);

  return inner;
}

*/
