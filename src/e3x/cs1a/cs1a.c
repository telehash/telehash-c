#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "sha256.h"
#include "cipher3.h"
#include "e3x.h"
#include "platform.h"

// cs1a local ones
#include "uECC.h"
#include "aes.h"
#include "hmac.h"

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
  uint8_t enckey[16], deckey[16];
  uint32_t seq;
} *ephemeral_t;

// these are all the locally implemented handlers defined in cipher3.h

static uint8_t *cipher_rand(uint8_t *s, uint32_t len);
static uint8_t *cipher_hash(uint8_t *input, uint32_t len, uint8_t *output);
static uint8_t *cipher_err(void);
static uint8_t cipher_generate(lob_t keys, lob_t secrets);

static local_t local_new(lob_t keys, lob_t secrets);
static void local_free(local_t local);
static lob_t local_decrypt(local_t local, lob_t outer);

static remote_t remote_new(lob_t key);
static void remote_free(remote_t remote);
static uint8_t remote_verify(remote_t remote, local_t local, lob_t outer);
static lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner);

static ephemeral_t ephemeral_new(remote_t remote, lob_t outer, lob_t inner);
static void ephemeral_free(ephemeral_t ephemeral);
static lob_t ephemeral_encrypt(ephemeral_t ephemeral, lob_t inner);
static lob_t ephemeral_decrypt(ephemeral_t ephemeral, lob_t outer);


static int RNG(uint8_t *p_dest, unsigned p_size)
{
  cipher_rand(p_dest,p_size);
  return 1;
}

cipher3_t cs1a_init(lob_t options)
{
  struct timeval tv;
  unsigned int seed;
  cipher3_t ret = malloc(sizeof(struct cipher3_struct));
  if(!ret) return NULL;
  memset(ret,0,sizeof (struct cipher3_struct));
  
  // identifying markers
  ret->id = CS_1a;
  ret->csid = 0x1a;
  memcpy(ret->hex,"1a",3);

  // normal init stuff
  gettimeofday(&tv, NULL);
  seed = (getpid() << 16) ^ tv.tv_sec ^ tv.tv_usec;
  srandom(seed); // srandomdev() is not universal
  uECC_set_rng(&RNG);

  // configure our callbacks
  ret->rand = cipher_rand;
  ret->hash = cipher_hash;
  ret->err = cipher_err;
  ret->generate = cipher_generate;

  // need to cast these to map our struct types to voids
  ret->local_new = (void *(*)(lob_t, lob_t))local_new;
  ret->local_free = (void (*)(void *))local_free;
  ret->local_decrypt = (lob_t (*)(void *, lob_t))local_decrypt;
  ret->remote_new = (void *(*)(lob_t))remote_new;
  ret->remote_free = (void (*)(void *))remote_free;
  ret->remote_verify = (uint8_t (*)(void *, void *, lob_t))remote_verify;
  ret->remote_encrypt = (lob_t (*)(void *, void *, lob_t))remote_encrypt;
  ret->ephemeral_new = (void *(*)(void *, lob_t, lob_t))ephemeral_new;
  ret->ephemeral_free = (void (*)(void *))ephemeral_free;
  ret->ephemeral_encrypt = (lob_t (*)(void *, lob_t))ephemeral_encrypt;
  ret->ephemeral_decrypt = (lob_t (*)(void *, lob_t))ephemeral_decrypt;

  return ret;
}

uint8_t *cipher_rand(uint8_t *s, uint32_t len)
{

 uint8_t *x = s;
 
  while(len-- > 0)
  {
    *x = (uint8_t)random();
    x++;
  }
  return s;
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
  if(!secret || secret->body_len != uECC_BYTES) return LOG("invalid secret");
  
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

remote_t remote_new(lob_t key)
{
  remote_t remote;
  if(!key || key->body_len != uECC_BYTES+1) return LOG("invalid key %d != %d",(key)?key->body_len:0,uECC_BYTES+1);
  
  if(!(remote = malloc(sizeof(struct remote_struct)))) return NULL;
  memset(remote,0,sizeof (struct remote_struct));

  // copy in key and make ephemeral ones
  uECC_decompress(key->body,remote->key);
  uECC_make_key(remote->ekey, remote->esecret);
  uECC_compress(remote->ekey, remote->ecomp);

  // generate a random seq starting point for message IV's
  e3x_rand((uint8_t*)&(remote->seq),4);
  
  return remote;
}

void remote_free(remote_t remote)
{
  free(remote);
}

#include "platform.h"
#include "util.h"

uint8_t remote_verify(remote_t remote, local_t local, lob_t outer)
{
  uint8_t shared[uECC_BYTES+4], hash[32];

  if(!remote || !local || !outer) return 1;
  if(outer->head_len != 1 || outer->head[0] != 0x1a) return 2;

  // generate the key for the hmac, combining the shared secret and IV
  if(!uECC_shared_secret(remote->key, local->secret, shared)) return 3;
  memcpy(shared+uECC_BYTES,outer->body+21,4);

  char hex[128];
  util_hex(shared,21+4,hex);
  LOG("SHARED %s",hex);

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

  char hex[128];
  util_hex(shared,21+4,hex);
  LOG("SHARED %s",hex);

  hmac_256(shared,uECC_BYTES+4,outer->body,21+4+inner_len,hash);
  fold3(hash,outer->body+21+4+inner_len); // write into last 4 bytes

  return outer;
}

ephemeral_t ephemeral_new(remote_t remote, lob_t outer, lob_t inner)
{
  return NULL;
}

void ephemeral_free(ephemeral_t ephemeral)
{
  free(ephemeral);
}

lob_t ephemeral_encrypt(ephemeral_t ephemeral, lob_t inner)
{
  return NULL;
}

lob_t ephemeral_decrypt(ephemeral_t ephemeral, lob_t outer)
{
  return NULL;
}
