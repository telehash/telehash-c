#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sodium.h>

#include "telehash.h"
#include "telehash.h"
#include "telehash.h"

// undefine the void* aliases so we can define them locally
#undef local_t
#undef remote_t
#undef ephemeral_t

typedef struct local_struct
{
  uint8_t secret[crypto_box_SECRETKEYBYTES], key[crypto_box_PUBLICKEYBYTES];
} *local_t;

typedef struct remote_struct
{
  uint8_t key[crypto_box_PUBLICKEYBYTES];
  uint8_t esecret[crypto_box_SECRETKEYBYTES], ekey[crypto_box_PUBLICKEYBYTES];
} *remote_t;

typedef struct ephemeral_struct
{
  uint8_t enckey[32], deckey[32], token[16];
} *ephemeral_t;

// these are all the locally implemented handlers defined in e3x_cipher.h

static uint8_t *cipher_hash(uint8_t *input, size_t len, uint8_t *output);
static uint8_t *cipher_err(void);
static uint8_t cipher_generate(lob_t keys, lob_t secrets);
static uint8_t *cipher_rand(uint8_t *bytes, size_t len);

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


e3x_cipher_t cs3a_init(lob_t options)
{
  e3x_cipher_t ret;

  // sanity check
  if(crypto_box_NONCEBYTES != 24 || crypto_box_PUBLICKEYBYTES != 32 || crypto_onetimeauth_BYTES != 16)
  {
    return LOG("libsodium version error, defined header sizes don't match expectations");
  }

  ret = malloc(sizeof(struct e3x_cipher_struct));
  if(!ret) return NULL;
  memset(ret,0,sizeof (struct e3x_cipher_struct));

  // identifying markers
  ret->id = CS_3a;
  ret->csid = 0x3a;
  memcpy(ret->hex,"3a",3);

  // which alg's we support
  ret->alg = "ED25519";

  // normal init stuff
  randombytes_stir();

  // configure our callbacks
  ret->hash = cipher_hash;
  ret->rand = cipher_rand;
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
  crypto_hash_sha256(output,input,(unsigned long)len);
  return output;
}

uint8_t *cipher_err(void)
{
  return 0;
}

uint8_t *cipher_rand(uint8_t *bytes, size_t len)
{
  randombytes_buf((void * const)bytes, len);
  return bytes;
}

uint8_t cipher_generate(lob_t keys, lob_t secrets)
{
  uint8_t secret[crypto_box_SECRETKEYBYTES], key[crypto_box_PUBLICKEYBYTES];

  // create identity keypair
  crypto_box_keypair(key,secret);

  lob_set_base32(keys,"3a",key,crypto_box_PUBLICKEYBYTES);
  lob_set_base32(secrets,"3a",secret,crypto_box_SECRETKEYBYTES);

  return 0;
}


local_t local_new(lob_t keys, lob_t secrets)
{
  local_t local;
  lob_t key, secret;

  if(!keys) keys = lob_linked(secrets); // for convenience
  key = lob_get_base32(keys,"3a");
  if(!key) return LOG("missing key");
  if(key->body_len != crypto_box_PUBLICKEYBYTES) return LOG("invalid key %d != %d",key->body_len,crypto_box_PUBLICKEYBYTES);

  secret = lob_get_base32(secrets,"3a");
  if(!secret) return LOG("missing secret");
  if(secret->body_len != crypto_box_SECRETKEYBYTES) return LOG("invalid secret %d != %d",secret->body_len,crypto_box_SECRETKEYBYTES);

  if(!(local = malloc(sizeof(struct local_struct)))) return NULL;
  memset(local,0,sizeof (struct local_struct));

  // copy in key/secret data
  memcpy(local->key,key->body,key->body_len);
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
  uint8_t secret[crypto_box_BEFORENMBYTES];
  lob_t inner, tmp;

//  * `KEY` - 32 bytes, the sending exchange's ephemeral public key
//  * `NONCE` - 24 bytes, randomly generated
//  * `CIPHERTEXT` - the inner packet bytes encrypted using secretbox() using the `NONCE` as the nonce and the shared secret (derived from the recipients endpoint key and the included ephemeral key) as the key
//  * `AUTH` - 16 bytes, the calculated onetimeauth(`KEY` + `INNER`, SHA256(`NONCE` + secret)) using the shared secret derived from both endpoint keys, the hashing is to minimize the chance that the same key input is ever used twice

  if(outer->body_len <= (32+24+crypto_secretbox_MACBYTES+16)) return NULL;
  tmp = lob_new();
  if(!lob_body(tmp,NULL,outer->body_len-(32+24+crypto_secretbox_MACBYTES+16))) return lob_free(tmp);

  // get the shared secret
  crypto_box_beforenm(secret, outer->body, local->secret);

  // decrypt the inner
  if(crypto_secretbox_open_easy(tmp->body,
    outer->body+32+24,
    tmp->body_len+crypto_secretbox_MACBYTES,
    outer->body+32,
    secret) != 0) return lob_free(tmp);

  // load inner packet
  inner = lob_parse(tmp->body,tmp->body_len);
  lob_free(tmp);

  return inner;
}

lob_t local_sign(local_t local, lob_t args, uint8_t *data, size_t len)
{
//  uint8_t hash[32];

  if(lob_get_cmp(args,"alg","ED25519") == 0)
  {
//    lob_body(args,NULL,32);
//    memcpy(args->body,hash,32);
//    return args;
  }

  return NULL;
}

remote_t remote_new(lob_t key, uint8_t *token)
{
  uint8_t hash[32];
  remote_t remote;

  if(!key) return LOG("missing key");
  if(key->body_len != crypto_box_PUBLICKEYBYTES) return LOG("invalid key %d != %d",key->body_len,crypto_box_PUBLICKEYBYTES);

  if(!(remote = malloc(sizeof(struct remote_struct)))) return NULL;
  memset(remote,0,sizeof (struct remote_struct));

  // copy in key and make ephemeral ones
  memcpy(remote->key,key->body,key->body_len);
  crypto_box_keypair(remote->ekey,remote->esecret);

  // set token if wanted
  if(token)
  {
    cipher_hash(remote->ekey,16,hash);
    memcpy(token,hash,16);
  }

  return remote;
}

void remote_free(remote_t remote)
{
  free(remote);
}

uint8_t remote_verify(remote_t remote, local_t local, lob_t outer)
{
  uint8_t secret[crypto_box_BEFORENMBYTES], shared[24+crypto_box_BEFORENMBYTES], hash[32];

  if(!remote || !local || !outer) return 1;
  if(outer->head_len != 1 || outer->head[0] != 0x3a) return 2;

  // generate secret and verify
  crypto_box_beforenm(secret, remote->key, local->secret);
  memcpy(shared,outer->body+32,24); // nonce
  memcpy(shared+24,secret,crypto_box_BEFORENMBYTES);
  e3x_hash(shared,24+crypto_box_BEFORENMBYTES,hash);
  if(crypto_onetimeauth_verify(outer->body+(outer->body_len-crypto_onetimeauth_BYTES),
    outer->body,
    outer->body_len-crypto_onetimeauth_BYTES,
    hash))
  {
    LOG("OTA verify failed");
    lob_free(outer);
    return 1;
  }

  return 0;
}

lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner)
{
  uint8_t secret[crypto_box_BEFORENMBYTES], nonce[24], shared[24+crypto_box_BEFORENMBYTES], hash[32], csid = 0x3a;
  lob_t outer;
  size_t inner_len;

  outer = lob_new();
  lob_head(outer,&csid,1);
  inner_len = lob_len(inner);
  if(!lob_body(outer,NULL,32+24+inner_len+crypto_secretbox_MACBYTES+16)) return lob_free(outer);

  // copy in the ephemeral public key/nonce
  memcpy(outer->body, remote->ekey, 32);
  randombytes(nonce,24);
  memcpy(outer->body+32, nonce, 24);

  // get the shared secret to create the nonce+key for the open aes
  crypto_box_beforenm(secret, remote->key, remote->esecret);

  // encrypt the inner
  if(crypto_secretbox_easy(outer->body+32+24,
    lob_raw(inner),
    inner_len,
    nonce,
    secret) != 0) return lob_free(outer);

  // generate secret for hmac
  crypto_box_beforenm(secret, remote->key, local->secret);
  memcpy(shared,nonce,24);
  memcpy(shared+24,secret,crypto_box_BEFORENMBYTES);
  e3x_hash(shared,24+crypto_box_BEFORENMBYTES,hash);
  crypto_onetimeauth(outer->body+32+24+inner_len+crypto_secretbox_MACBYTES, outer->body, outer->body_len-16, hash);

  return outer;
}

uint8_t remote_validate(remote_t remote, lob_t args, lob_t sig, uint8_t *data, size_t len)
{
//  uint8_t hash[32];
  if(!args || !sig || !data || !len) return 1;

  if(lob_get_cmp(args,"alg","ED25519") == 0)
  {
//    if(sig->body_len != 32 || !args->body_len) return 2;
//    LOG("[%d] [%.*s]",args->body_len,len,data);
//    hmac_256(args->body,args->body_len,data,len,hash);
//    return (util_ct_memcmp(sig->body,hash,32) == 0) ? 0 : 3;
  }

  return 3;
}

ephemeral_t ephemeral_new(remote_t remote, lob_t outer)
{
  uint8_t shared[crypto_box_BEFORENMBYTES+(crypto_box_PUBLICKEYBYTES*2)], hash[32];
  ephemeral_t ephem;

  if(!remote) return NULL;
  if(!outer || outer->body_len < crypto_box_PUBLICKEYBYTES) return LOG("invalid outer");

  if(!(ephem = malloc(sizeof(struct ephemeral_struct)))) return NULL;
  memset(ephem,0,sizeof (struct ephemeral_struct));

  // create and copy in the exchange routing token
  e3x_hash(outer->body,16,hash);
  memcpy(ephem->token,hash,16);

  // do the diffie hellman
  crypto_box_beforenm(shared, outer->body, remote->esecret);

  // combine inputs to create the digest-derived keys
  memcpy(shared+crypto_box_BEFORENMBYTES,remote->ekey,crypto_box_PUBLICKEYBYTES);
  memcpy(shared+crypto_box_BEFORENMBYTES+crypto_box_PUBLICKEYBYTES,outer->body,crypto_box_PUBLICKEYBYTES);
  e3x_hash(shared,crypto_box_BEFORENMBYTES+(crypto_box_PUBLICKEYBYTES*2),ephem->enckey);

  memcpy(shared+crypto_box_BEFORENMBYTES,outer->body,crypto_box_PUBLICKEYBYTES);
  memcpy(shared+crypto_box_BEFORENMBYTES+crypto_box_PUBLICKEYBYTES,remote->ekey,crypto_box_PUBLICKEYBYTES);
  e3x_hash(shared,crypto_box_BEFORENMBYTES+(crypto_box_PUBLICKEYBYTES*2),ephem->deckey);

  return ephem;
}

void ephemeral_free(ephemeral_t ephem)
{
  free(ephem);
}

lob_t ephemeral_encrypt(ephemeral_t ephem, lob_t inner)
{
  lob_t outer;
  size_t inner_len;

  outer = lob_new();
  inner_len = lob_len(inner);
  if(!lob_body(outer,NULL,16+24+inner_len+crypto_secretbox_MACBYTES)) return lob_free(outer);

  // copy in token and create nonce
  memcpy(outer->body,ephem->token,16);
  randombytes(outer->body+16,24);

  crypto_secretbox_easy(outer->body+16+24,
    lob_raw(inner),
    lob_len(inner),
    outer->body+16,
    ephem->enckey);

  return outer;

}

lob_t ephemeral_decrypt(ephemeral_t ephem, lob_t outer)
{
  crypto_secretbox_open_easy(outer->body+16+24,
    outer->body+16+24,
    outer->body_len-(16+24),
    outer->body+16,
    ephem->deckey);

  return lob_parse(outer->body+16+24, outer->body_len-(16+24+crypto_secretbox_MACBYTES));
}
