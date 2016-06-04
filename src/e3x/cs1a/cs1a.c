#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "telehash.h"

#define KEY_BYTES 40
#define COMP_BYTES 21
#define SECRET_BYTES 21
#define SHARED_BYTES 20
#define curve uECC_secp160r1()

// undefine the void* aliases so we can define them locally
#undef local_t
#undef remote_t
#undef ephemeral_t

typedef struct local_struct
{
  uint8_t secret[SECRET_BYTES], key[KEY_BYTES];
} *local_t;

typedef struct remote_struct
{
  uint8_t key[KEY_BYTES];
  uint8_t esecret[SECRET_BYTES], ekey[KEY_BYTES], ecomp[COMP_BYTES];
  uint32_t seq;
} *remote_t;

typedef struct ephemeral_struct
{
  uint8_t enckey[16], deckey[16], token[16];
  uint32_t seq;
} *ephemeral_t;

// these are all the locally implemented handlers defined in e3x_cipher.h

static uint8_t *cipher_hash(uint8_t *input, size_t len, uint8_t *output);
static uint8_t *cipher_err(void);
static uint8_t cipher_generate(lob_t keys, lob_t secrets);

static local_t local_new(lob_t keys, lob_t secrets);
static void local_free(local_t local);
static lob_t local_decrypt(local_t local, lob_t outer);
static lob_t local_sign(local_t local, lob_t args, uint8_t *data, size_t len);

static remote_t remote_new(lob_t key, uint8_t *token);
static void remote_free(remote_t remote);
static uint8_t remote_verify(remote_t remote, local_t local, lob_t outer);
static lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner);
static uint8_t remote_validate(remote_t remote, lob_t args, lob_t sig, uint8_t *data, size_t len);

static ephemeral_t ephemeral_new(remote_t remote, lob_t outer);
static void ephemeral_free(ephemeral_t ephemeral);
static lob_t ephemeral_encrypt(ephemeral_t ephemeral, lob_t inner);
static lob_t ephemeral_decrypt(ephemeral_t ephemeral, lob_t outer);


static int RNG(uint8_t *p_dest, unsigned p_size)
{
  e3x_rand(p_dest,p_size);
  return 1;
}

e3x_cipher_t cs1a_init(lob_t options)
{
  e3x_cipher_t ret = malloc(sizeof(struct e3x_cipher_struct));
  if(!ret) return NULL;
  memset(ret,0,sizeof (struct e3x_cipher_struct));

  // identifying markers
  ret->id = CS_1a;
  ret->csid = 0x1a;
  memcpy(ret->hex,"1a",3);

  // which alg's we support
  ret->alg = "HS256 ES160";

  // normal init stuff
  uECC_set_rng(&RNG);

  // configure our callbacks (no RNG, default to platform's)
  ret->hash = cipher_hash;
  ret->err = cipher_err;
  ret->generate = cipher_generate;

  // need to cast these to map our struct types to voids
  ret->local_new = (void *(*)(lob_t, lob_t))local_new;
  ret->local_free = (void (*)(void *))local_free;
  ret->local_decrypt = (lob_t (*)(void *, lob_t))local_decrypt;
  ret->local_sign = (lob_t (*)(void *, lob_t, uint8_t *, size_t))local_sign;
  ret->remote_new = (void *(*)(lob_t, uint8_t *))remote_new;
  ret->remote_free = (void (*)(void *))remote_free;
  ret->remote_verify = (uint8_t (*)(void *, void *, lob_t))remote_verify;
  ret->remote_encrypt = (lob_t (*)(void *, void *, lob_t))remote_encrypt;
  ret->remote_validate = (uint8_t (*)(void *, lob_t, lob_t, uint8_t *, size_t))remote_validate;
  ret->ephemeral_new = (void *(*)(void *, lob_t))ephemeral_new;
  ret->ephemeral_free = (void (*)(void *))ephemeral_free;
  ret->ephemeral_encrypt = (lob_t (*)(void *, lob_t))ephemeral_encrypt;
  ret->ephemeral_decrypt = (lob_t (*)(void *, lob_t))ephemeral_decrypt;

  return ret;
}

uint8_t *cipher_hash(uint8_t *input, size_t len, uint8_t *output)
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
  uint8_t secret[SECRET_BYTES], key[KEY_BYTES], comp[COMP_BYTES];

  if(!uECC_make_key(key, secret, curve)) return 1;
  uECC_compress(key,comp, curve);
  lob_set_base32(keys,"1a",comp,COMP_BYTES);
  lob_set_base32(secrets,"1a",secret,SECRET_BYTES);

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
  local_t local = NULL;
  lob_t key, secret;

  if(!keys) keys = lob_linked(secrets); // for convenience
  key = lob_get_base32(keys,"1a");
  if(!key) return LOG("invalid key");

  secret = lob_get_base32(secrets,"1a");
  if(!secret) return LOG("invalid secret");

  if(key->body_len == COMP_BYTES && (secret->body_len == SECRET_BYTES || secret->body_len == (SECRET_BYTES-1)))
  {
    if((local = malloc(sizeof(struct local_struct))))
    {
      memset(local,0,sizeof (struct local_struct));

      // copy in key/secret data
      uECC_decompress(key->body,local->key, curve);

      // handle old format missing prepended null byte
      if(secret->body_len == SECRET_BYTES-1)
      {
        memcpy(local->secret+1,secret->body,secret->body_len);
      }else{
        memcpy(local->secret,secret->body,secret->body_len);
      }
      
    }else{
      LOG("OOM");
    }
    
  }else{
    LOG("invalid sizes key %d=%d secret %d=%d",key->body_len,COMP_BYTES,secret->body_len,SECRET_BYTES);
  }

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
  uint8_t key[KEY_BYTES], shared[SECRET_BYTES], iv[16], hash[32];
  lob_t inner, tmp;

//  * `KEY` - 21 bytes, the sender's ephemeral exchange public key in compressed format
//  * `IV` - 4 bytes, a random but unique value determined by the sender
//  * `INNER` - (minimum 21+2 bytes) the AES-128-CTR encrypted inner packet ciphertext
//  * `HMAC` - 4 bytes, the calculated HMAC of all of the previous KEY+INNER bytes

  if(outer->body_len <= (21+4+0+4)) return NULL;
  tmp = lob_new();
  if(!lob_body(tmp,NULL,outer->body_len-(4+21+4))) return lob_free(tmp);

  // get the shared secret to create the iv+key for the open aes
  uECC_decompress(outer->body,key, curve);
  if(!uECC_shared_secret(key, local->secret, shared, curve)) return lob_free(tmp);
  e3x_hash(shared,SHARED_BYTES,hash);
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

lob_t local_sign(local_t local, lob_t args, uint8_t *data, size_t len)
{
  uint8_t hash[32], sig[KEY_BYTES];

  if(lob_get_cmp(args,"alg","HS256") == 0)
  {
//    LOG("[%.*s] [%.*s]",args->body_len,args->body,len,data);
    hmac_256(args->body,args->body_len,data,len,hash);
    lob_body(args,NULL,32);
    memcpy(args->body,hash,32);
    return args;
  }

  if(lob_get_cmp(args,"alg","ES160") == 0)
  {
    if(!local) return NULL;
    // hash data first, then sign it
    e3x_hash(data,len,hash);
    uECC_sign(local->secret,hash,32,sig, curve);
    lob_body(args,NULL,KEY_BYTES);
    memcpy(args->body,sig,KEY_BYTES);
    return args;
  }

  return NULL;
}

remote_t remote_new(lob_t key, uint8_t *token)
{
  uint8_t hash[32];
  remote_t remote;
  if(!key || key->body_len != COMP_BYTES) return LOG("invalid key %d != %d",(key)?key->body_len:0,COMP_BYTES);

  if(!(remote = malloc(sizeof(struct remote_struct)))) return NULL;
  memset(remote,0,sizeof (struct remote_struct));

  // copy in key and make ephemeral ones
  uECC_decompress(key->body,remote->key, curve);
  uECC_make_key(remote->ekey, remote->esecret, curve);
  uECC_compress(remote->ekey, remote->ecomp, curve);
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
  uint8_t shared[SHARED_BYTES+4], hash[32];

  if(!remote || !local || !outer) return 1;
  if(outer->head_len != 1 || outer->head[0] != 0x1a) return 2;

  // generate the key for the hmac, combining the shared secret and IV
  if(!uECC_shared_secret(remote->key, local->secret, shared, curve)) return 3;
  memcpy(shared+SHARED_BYTES,outer->body+21,4);

  // verify
  hmac_256(shared,SHARED_BYTES+4,outer->body,outer->body_len-4,hash);
  fold3(hash,hash);
  if(util_ct_memcmp(hash,outer->body+(outer->body_len-4),4) != 0)
  {
    LOG("hmac failed");
    return 4;
  }

  return 0;
}

lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner)
{
  uint8_t shared[SHARED_BYTES+4], iv[16], hash[32], csid = 0x1a;
  lob_t outer;
  size_t inner_len;

  outer = lob_new();
  lob_head(outer,&csid,1);
  inner_len = lob_len(inner);
  if(!lob_body(outer,NULL,21+4+inner_len+4)) return lob_free(outer);

  // copy in the ephemeral public key
  memcpy(outer->body, remote->ecomp, COMP_BYTES);

  // get the shared secret to create the iv+key for the open aes
  if(!uECC_shared_secret(remote->key, remote->esecret, shared, curve)) return lob_free(outer);
  e3x_hash(shared,SHARED_BYTES,hash);
  fold1(hash,hash);
  memset(iv,0,16);
  memcpy(iv,&(remote->seq),4);
  remote->seq++; // increment seq after every use
  memcpy(outer->body+21,iv,4); // send along the used IV

  // encrypt the inner into the outer
  aes_128_ctr(hash,inner_len,iv,lob_raw(inner),outer->body+21+4);

  // generate secret for hmac
  if(!uECC_shared_secret(remote->key, local->secret, shared, curve)) return lob_free(outer);
  memcpy(shared+SHARED_BYTES,outer->body+21,4); // use the IV too

  hmac_256(shared,SHARED_BYTES+4,outer->body,21+4+inner_len,hash);
  fold3(hash,outer->body+21+4+inner_len); // write into last 4 bytes

  return outer;
}

uint8_t remote_validate(remote_t remote, lob_t args, lob_t sig, uint8_t *data, size_t len)
{
  uint8_t hash[32];
  if(!args || !sig || !data || !len) return 1;

  if(lob_get_cmp(args,"alg","HS256") == 0)
  {
    if(sig->body_len != 32 || !args->body_len) return 2;
    hmac_256(args->body,args->body_len,data,len,hash);
    return (util_ct_memcmp(sig->body,hash,32) == 0) ? 0 : 3;
  }

  if(lob_get_cmp(args,"alg","ES160") == 0)
  {
    if(!remote || sig->body_len != KEY_BYTES) return 2;
    // hash data first
    e3x_hash(data,len,hash);
    return (uECC_verify(remote->key, hash, 32, sig->body, curve) == 1) ? 0 : 3;
  }

  return 3;
}

ephemeral_t ephemeral_new(remote_t remote, lob_t outer)
{
  uint8_t ekey[KEY_BYTES], shared[SHARED_BYTES+((COMP_BYTES)*2)], hash[32];
  ephemeral_t ephem;

  if(!remote) return NULL;
  if(!outer || outer->body_len < (COMP_BYTES)) return LOG("invalid outer");

  if(!(ephem = malloc(sizeof(struct ephemeral_struct)))) return NULL;
  memset(ephem,0,sizeof (struct ephemeral_struct));

  // create and copy in the exchange routing token
  e3x_hash(outer->body,16,hash);
  memcpy(ephem->token,hash,16);

  // generate a random seq starting point for channel IV's
  e3x_rand((uint8_t*)&(ephem->seq),4);

  // decompress the exchange key and get the shared secret
  uECC_decompress(outer->body,ekey, curve);
  if(!uECC_shared_secret(ekey, remote->esecret, shared, curve))
  {
    ephemeral_free(ephem);
    return LOG("ECDH failed");
  }

  // combine inputs to create the digest
  memcpy(shared+SHARED_BYTES,remote->ecomp,COMP_BYTES);
  memcpy(shared+SHARED_BYTES+COMP_BYTES,outer->body,COMP_BYTES);
  e3x_hash(shared,SHARED_BYTES+((COMP_BYTES)*2),hash);
  fold1(hash,ephem->enckey);

  memcpy(shared+SHARED_BYTES,outer->body,COMP_BYTES);
  memcpy(shared+SHARED_BYTES+COMP_BYTES,remote->ecomp,COMP_BYTES);
  e3x_hash(shared,SHARED_BYTES+((COMP_BYTES)*2),hash);
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
  size_t inner_len;

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

  if(util_ct_memcmp(hmac,outer->body+(outer->body_len-4),4) != 0) return LOG("hmac failed");

  // decrypt in place
  aes_128_ctr(ephem->deckey,outer->body_len-(16+4+4),iv,outer->body+16+4,outer->body+16+4);

  // return parse attempt
  return lob_parse(outer->body+16+4, outer->body_len-(16+4+4));
}
