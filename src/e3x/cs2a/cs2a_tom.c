#include <tomcrypt.h>
#include <tommath.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "telehash.h"
#include "telehash.h"
#include "telehash.h"

// TODO, there are leaks in the usage of libtomcrypt here, but nobody's using it yet

// undefine the void* aliases so we can define them locally
#undef local_t
#undef remote_t
#undef ephemeral_t

typedef struct local_struct
{
  rsa_key rsa;
} *local_t;

typedef struct remote_struct
{
  uint8_t keys[256], iv[12], cached[65+32];
  uint8_t enckey[32];
  ecc_key ecc;
  rsa_key rsa;
  gcm_state enc;
} *remote_t;

typedef struct ephemeral_struct
{
  gcm_state enc, dec;
  uint8_t token[16], iv[12];
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

static prng_state _libtom_prng;
static int _libtom_err;

#define TOM_IF(x) if ((_libtom_err = x) != CRYPT_OK)
#define TOM_OK(x) if ((_libtom_err = x) != CRYPT_OK) {return LOG("lobtom error: %s",error_to_string(_libtom_err));}
#define TOM_OK2(x,y) if ((_libtom_err = x) != CRYPT_OK) {y;return LOG("lobtom error: %s",error_to_string(_libtom_err));}

e3x_cipher_t cs2a_init(lob_t options)
{
  e3x_cipher_t ret = malloc(sizeof(struct e3x_cipher_struct));
  if(!ret) return NULL;
  memset(ret,0,sizeof (struct e3x_cipher_struct));
  
  // identifying markers
  ret->id = CS_2a;
  ret->csid = 0x2a;
  memcpy(ret->hex,"2a",3);

  // which alg's we support
  ret->alg = "RS256 ES256";

  // normal init stuff
  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);
  TOM_OK(rng_make_prng(128, find_prng("yarrow"), &_libtom_prng, NULL));

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
  unsigned long hlen = 32;
  TOM_OK(hash_memory(find_hash("sha256"), input, len, output, &hlen));
  return output;
}

uint8_t *cipher_err(void)
{
  if(_libtom_err == CRYPT_OK) return 0;
  return (uint8_t*)error_to_string(_libtom_err);
}

uint8_t *cipher_rand(uint8_t *bytes, size_t len)
{
  if(yarrow_read((unsigned char*)bytes, len, &_libtom_prng) != len) return LOG("couldn't get enough random");
  return bytes;
}

uint8_t cipher_generate(lob_t keys, lob_t secrets)
{
  unsigned long len;
  unsigned char *buf;
  rsa_key rsa;

  TOM_IF(rsa_make_key(&_libtom_prng, find_prng("yarrow"), 256, 65537, &rsa)) return 2;

  // this determines the size needed into len
  len = 1;
  buf = malloc(len);
  rsa_export(buf, &len, PK_PRIVATE, &rsa);
  if(!(buf = util_reallocf(buf,len))) return 2;

  // export private
  TOM_IF(rsa_export(buf, &len, PK_PRIVATE, &rsa))
  {
    free(buf);
    return 3;
  }
  lob_set_base32(secrets,"2a",buf,len);

  TOM_IF(rsa_export(buf, &len, PK_PUBLIC, &rsa))
  {
    free(buf);
    return 4;
  }
  lob_set_base32(keys,"2a",buf,len);
  free(buf);

  return 0;
}

local_t local_new(lob_t keys, lob_t secrets)
{
  local_t local;
  lob_t key, secret;

  if(!keys) keys = lob_linked(secrets); // for convenience
  key = lob_get_base32(keys,"2a");
  secret = lob_get_base32(secrets,"2a");
  if(!key || !secret) return LOG("invalid 2a key/secret");
  
  if(!(local = malloc(sizeof(struct local_struct)))) return NULL;
  memset(local,0,sizeof (struct local_struct));

  TOM_IF(rsa_import(secret->body, secret->body_len, &(local->rsa)))
  {
    LOG("invalid rsa key");
    free(local);
    local = NULL;
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

static lob_t tmp_decrypt(local_t local, lob_t outer)
{
  lob_t tmp;
  unsigned long len;
  int res;
  gcm_state dec;

  if(!outer || outer->body_len <= (256+12+256+16)) return LOG("too small");

  tmp = lob_new();
  if(!lob_head(tmp,NULL,65+32)) return lob_free(tmp);
  if(!lob_body(tmp,NULL,outer->body_len-(256+12+16))) return lob_free(tmp);
  
//  * `KEYS` - 256 bytes, PKCS1 OAEP v2 RSA encrpyted ciphertext of the 65 byte uncompressed ECC P-256 ephemeral public key and 32 byte secret gcm key
//  * `IV` - 12 bytes, a random but unique value determined by the sender for each message
//  * `CIPHERTEXT` - AES-256-GCM encrypted inner packet and sender signature
//  * `MAC` - 16 bytes, GCM 128-bit MAC/tag digest
//  The `CIPHERTEXT` once deciphered contains:
//  * `INNER` - inner packet raw bytes
//  * `SIG` - 256 bytes, PKCS1 v1.5 RSA signature of the KEY+IV+INNER

  // decrypt the KEYS into the head
  len = tmp->head_len;
  TOM_OK2(rsa_decrypt_key(outer->body, 256, tmp->head, &len, 0, 0, find_hash("sha1"), &res, &(local->rsa)), lob_free(tmp));

  // decrypt the inner+sig into the body
  TOM_OK2(gcm_init(&dec, find_cipher("aes"), tmp->head+65, 32), lob_free(tmp));
  TOM_OK2(gcm_add_iv(&dec, outer->body+256, 12), lob_free(tmp));
  TOM_OK2(gcm_add_aad(&dec, outer->body, 256), lob_free(tmp));
  TOM_OK2(gcm_process(&dec, tmp->body, tmp->body_len, outer->body+256+12, GCM_DECRYPT), lob_free(tmp));
  len = 16;
  TOM_OK2(gcm_done(&dec, outer->body+(outer->body_len-16), &len), lob_free(tmp));

  return tmp;
}

lob_t local_decrypt(local_t local, lob_t outer)
{
  lob_t inner, tmp;

  if(!(tmp = tmp_decrypt(local, outer))) return LOG("decrypt failed");

  // parse a packet out of the decrypted bytes minus the signature
  if((inner = lob_parse(tmp->body,tmp->body_len-256)) == NULL) return lob_free(tmp);
  lob_free(tmp);

  return inner;
}

lob_t local_sign(local_t local, lob_t args, uint8_t *data, size_t len)
{
  uint8_t hash[32];
  unsigned long len2; 

  if(lob_get_cmp(args,"alg","RS256") == 0)
  {
    if(!lob_body(args,NULL,256)) return LOG("OOM");
    cipher_hash(data,len,hash);
    len2 = 256;
    TOM_OK(rsa_sign_hash_ex(hash, 32, args->body, &len2, LTC_PKCS_1_V1_5, &_libtom_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(local->rsa)));
    return args;
  }

  if(lob_get_cmp(args,"alg","ES256") == 0)
  {
    // TODO pass in ecc private key to use
//    if(!lob_body(args,NULL,32)) return LOG("OOM");
//    cipher_hash(data,len,hash);
//    len2 = 32;
//    TOM_OK(ecc_sign_hash(hash, 32, args->body, &len2, &_libtom_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(local->rsa)));
//    return args;
  }

  return NULL;
}

remote_t remote_new(lob_t key, uint8_t *token)
{
  uint8_t hash[32], upub[65+32];
  unsigned long ulen, elen;
  remote_t remote;

  if(!key || !key->body_len) return LOG("bad key");

  if(!(remote = malloc(sizeof(struct remote_struct)))) return LOG("OOM");
  memset(remote,0,sizeof (struct remote_struct));

  // import given rsa key
  TOM_OK2(rsa_import(key->body, key->body_len, &(remote->rsa)), free(remote));
  
  // create ephemeral key
  TOM_OK2(ecc_make_key(&_libtom_prng, find_prng("yarrow"), 32, &(remote->ecc)), free(remote));
  
  // export the ephemeral uncompressed ecc key
  ulen = sizeof(upub);
  TOM_OK2(ecc_ansi_x963_export(&(remote->ecc), upub, &ulen), free(remote));
  
  // generate/append a random secret key
  e3x_rand(remote->enckey, 32);
  memcpy(upub+ulen, remote->enckey, 32);

  // create the gcm for message encryption
  TOM_OK2(gcm_init(&remote->enc, find_cipher("aes"), remote->enckey, 32), free(remote));

  // rsa encrypt the keys (PKCS1 OAEP v2) 
  elen = sizeof(remote->keys);
  TOM_OK2(rsa_encrypt_key(upub, ulen+32, remote->keys, &elen, 0, 0, &_libtom_prng, find_prng("yarrow"), find_hash("sha1"), &(remote->rsa)), free(remote));
  
  // generate a random seq starting point for message IV's
  e3x_rand(remote->iv,12);

  // return token if requested
  if(token)
  {
    cipher_hash(remote->keys,16,hash);
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
  lob_t tmp;
  uint8_t hash[32];
  size_t inner_len;
  hash_state md;
  int res, sha = find_hash("sha256");

  if(!remote || !local || !outer) return 1;
  if(outer->head_len != 1 || outer->head[0] != 0x2a)
  {
    LOG("invalid message header");
    return 2;
  }

  // get the decrypted bytes
  if(!(tmp = tmp_decrypt(local, outer))) return 4;
  inner_len = tmp->body_len-256;
  
  // hash both parts for the sig
  hash_descriptor[sha].init(&md);
  hash_descriptor[sha].process(&md, outer->body, 256+12);
  hash_descriptor[sha].process(&md, tmp->body, inner_len);
  hash_descriptor[sha].done(&md, hash);

  TOM_IF(rsa_verify_hash_ex(tmp->body+inner_len, 256, hash, 32, LTC_PKCS_1_V1_5, find_hash("sha256"), 0, &res, &(remote->rsa)))
  {
    LOG("rsa verify failed: %s",error_to_string(_libtom_err));
    lob_free(tmp);
    return 6;
  }
  
  // cache decrypted keys bytes for use in ephemeral setup
  memcpy(remote->cached, tmp->head, tmp->head_len);

  return 0;
}

lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner)
{
  uint8_t hash[32], csid = 0x2a;
  lob_t outer;
  size_t inner_len;
  unsigned long len;

  outer = lob_new();
  lob_head(outer,&csid,1);
  inner_len = lob_len(inner);

//  * `KEYS` - 256 bytes, PKCS1 OAEP v2 RSA encrpyted ciphertext of the 65 byte uncompressed ECC P-256 ephemeral public key and 32 byte secret gcm key
//  * `IV` - 12 bytes, a random but unique value determined by the sender for each message
//  * `CIPHERTEXT` - AES-256-GCM encrypted inner packet and sender signature
//  * `MAC` - 16 bytes, GCM 128-bit MAC/tag digest
//  The `CIPHERTEXT` once deciphered contains:
//  * `INNER` - inner packet raw bytes
//  * `SIG` - 256 bytes, PKCS1 v1.5 RSA signature of the KEY+IV+INNER

  if(!lob_body(outer,NULL,256+12+inner_len+256+16)) return lob_free(outer);

  // copy in the ephemeral encrypted public key
  memcpy(outer->body, remote->keys, 256);

  // change the iv and copy it in too
  (*(unsigned long *)remote->iv)++;
  memcpy(outer->body+256,remote->iv,12);

  // copy in the raw inner in order to generate the signature
  memcpy(outer->body+256+12,lob_raw(inner),inner_len);
  
  // now, sign everything so far, appended after the raw inner
  cipher_hash(outer->body,256+12+inner_len,hash);
  len = 256;
  TOM_OK2(rsa_sign_hash_ex(hash, 32, outer->body+256+12+inner_len, &len, LTC_PKCS_1_V1_5, &_libtom_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(local->rsa)), lob_free(outer));

  // lastly, aes-gcm encrypt the inner+sig in place
  TOM_OK2(gcm_reset(&(remote->enc)), lob_free(outer));
  TOM_OK2(gcm_add_iv(&(remote->enc), remote->iv, 12), lob_free(outer));
  TOM_OK2(gcm_add_aad(&(remote->enc), outer->body, 256), lob_free(outer));
  TOM_OK2(gcm_process(&(remote->enc), outer->body+256+12, inner_len+256, outer->body+256+12, GCM_ENCRYPT), lob_free(outer));
  len = 16;
  TOM_OK2(gcm_done(&(remote->enc), outer->body+256+12+inner_len+256, &len), lob_free(outer));

  return outer;
}

uint8_t remote_validate(remote_t remote, lob_t args, lob_t sig, uint8_t *data, size_t len)
{
  uint8_t hash[32];
  int res;
  if(!args || !sig || !data || !len) return 1;

  if(lob_get_cmp(args,"alg","RS256") == 0)
  {
    if(!remote || sig->body_len != 256) return 2;
    cipher_hash(data,len,hash);
    TOM_IF(rsa_verify_hash_ex(sig->body, 256, hash, 32, LTC_PKCS_1_V1_5, find_hash("sha256"), 0, &res, &(remote->rsa)))
    {
      LOG("rsa verify failed: %s",error_to_string(_libtom_err));
      return 3;
    }
    return 0;
  }

  return 4;
}

ephemeral_t ephemeral_new(remote_t remote, lob_t outer)
{
  uint8_t secret[32], hash[32];
  ephemeral_t ephem;
  unsigned long len;
  ecc_key ecc;
  hash_state md;
  int sha = find_hash("sha256");

  if(!remote || !outer || outer->body_len < 16) return NULL;

  if(!(ephem = malloc(sizeof(struct ephemeral_struct)))) return NULL;
  memset(ephem,0,sizeof (struct ephemeral_struct));

  // generate a random seq starting point for channel IV's
  e3x_rand(ephem->iv,12);
  
  // create and copy in the exchange routing token
  e3x_hash(outer->body,16,hash);
  memcpy(ephem->token,hash,16);

  // import ecc public key and do ECDH to get the shared secret
//  LOG("cached %s",util_hex(remote->cached,65+32,NULL));
  TOM_OK2(ecc_ansi_x963_import(remote->cached, 65, &ecc), free(ephem));
  len = sizeof(secret);
  TOM_OK2(ecc_shared_secret(&(remote->ecc),&ecc,secret,&len), free(ephem));

  // hash inputs to create the encryption gcm
  hash_descriptor[sha].init(&md);
  hash_descriptor[sha].process(&md, secret, 32);
  hash_descriptor[sha].process(&md, remote->enckey, 32);
  hash_descriptor[sha].process(&md, remote->cached+65, 32);
  hash_descriptor[sha].done(&md, hash);
  TOM_OK2(gcm_init(&ephem->enc, find_cipher("aes"), hash, 32), free(ephem));

  // hash inputs to create the decryption gcm
  hash_descriptor[sha].init(&md);
  hash_descriptor[sha].process(&md, secret, 32);
  hash_descriptor[sha].process(&md, remote->cached+65, 32);
  hash_descriptor[sha].process(&md, remote->enckey, 32);
  hash_descriptor[sha].done(&md, hash);
  TOM_OK2(gcm_init(&ephem->dec, find_cipher("aes"), hash, 32), free(ephem));

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
  unsigned long len;

  outer = lob_new();
  inner_len = lob_len(inner);
  if(!lob_body(outer,NULL,16+12+inner_len+16)) return lob_free(outer);

  // copy in token and update/copy iv
  memcpy(outer->body,ephem->token,16);
  (*(unsigned long *)ephem->iv)++;
  memcpy(outer->body+16,ephem->iv,12);

  // aes gcm encrypt into outer
  TOM_OK2(gcm_reset(&(ephem->enc)), lob_free(outer));
  TOM_OK2(gcm_add_iv(&(ephem->enc), ephem->iv, 12), lob_free(outer));
  gcm_add_aad(&(ephem->enc), NULL, 0);
  TOM_OK2(gcm_process(&(ephem->enc), lob_raw(inner), inner_len, outer->body+16+12, GCM_ENCRYPT), lob_free(outer));
  len = 16;
  TOM_OK2(gcm_done(&(ephem->enc), outer->body+16+12+inner_len, &len), lob_free(outer));
  
  return outer;
}

lob_t ephemeral_decrypt(ephemeral_t ephem, lob_t outer)
{
  unsigned long len;
  size_t inner_len = outer->body_len-(16+12+16);

  // decrypt in place
  TOM_OK(gcm_reset(&(ephem->dec)));
  TOM_OK(gcm_add_iv(&(ephem->dec), outer->body+16, 12));
  gcm_add_aad(&(ephem->dec), NULL, 0);
  TOM_OK(gcm_process(&(ephem->dec), outer->body+16+12, inner_len, outer->body+16+12, GCM_DECRYPT));
  len = 16;
  TOM_OK(gcm_done(&(ephem->dec), outer->body+16+12+inner_len, &len));

  // return parse attempt
  return lob_parse(outer->body+16+12, inner_len);

}

