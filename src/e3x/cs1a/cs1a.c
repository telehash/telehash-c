#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "sha256.h"
#include "cipher3.h"
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

local_t local_new(lob_t keys, lob_t secrets)
{
  local_t local;
  lob_t key = lob_get_base32(keys,"1a");
  lob_t secret = lob_get_base32(secrets,"1a");
  if(!key || key->body_len != uECC_BYTES+1) return LOG("invalid key %d != %d",(key)?key->body_len:0,uECC_BYTES+1);
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
  return NULL;
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

  return remote;
}

void remote_free(remote_t remote)
{
  free(remote);
}

uint8_t remote_verify(remote_t remote, local_t local, lob_t outer)
{
  return 1;
}

lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner)
{
  return NULL;
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
