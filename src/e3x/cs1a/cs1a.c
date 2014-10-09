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
  uint8_t id_private[uECC_BYTES], id_public[uECC_BYTES *2];
} *local_t;

typedef struct remote_struct
{
  uint8_t line_private[uECC_BYTES], line_public[uECC_BYTES *2];
} *remote_t;

typedef struct ephemeral_struct
{
  unsigned char keyOut[16], keyIn[16];
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
  uint8_t id_private[uECC_BYTES], id_public[uECC_BYTES*2];

  if(!uECC_make_key(id_public, id_private)) return 1;
  lob_set_base32(keys,"1a",id_public,uECC_BYTES*2);
  lob_set_base32(secrets,"1a",id_private,uECC_BYTES);

  return 0;
}

local_t local_new(lob_t keys, lob_t secrets)
{
  local_t local;
  if(!(local = malloc(sizeof(struct local_struct)))) return NULL;
  memset(local,0,sizeof (struct local_struct));
  return local;
}

void local_free(local_t local)
{
  return;
}

lob_t local_decrypt(local_t local, lob_t outer)
{
  return NULL;
}

remote_t remote_new(lob_t key)
{
  return NULL;
}

void remote_free(remote_t remote)
{
  
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

}

lob_t ephemeral_encrypt(ephemeral_t ephemeral, lob_t inner)
{
  return NULL;
}

lob_t ephemeral_decrypt(ephemeral_t ephemeral, lob_t outer)
{
  return NULL;
}
