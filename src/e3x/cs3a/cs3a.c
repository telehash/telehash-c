#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sodium.h>

#include "e3x.h"
#include "e3x_cipher.h"
#include "util_sys.h"

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

static remote_t remote_new(lob_t key, uint8_t *token);
static void remote_free(remote_t remote);
static uint8_t remote_verify(remote_t remote, local_t local, lob_t outer);
static lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner);

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
  ret->remote_new = (void *(*)(lob_t, uint8_t *))remote_new;
  ret->remote_free = (void (*)(void *))remote_free;
  ret->remote_verify = (uint8_t (*)(void *, void *, lob_t))remote_verify;
  ret->remote_encrypt = (lob_t (*)(void *, void *, lob_t))remote_encrypt;
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
  uint8_t nonce[crypto_box_NONCEBYTES], secret[crypto_box_BEFORENMBYTES];
  lob_t inner, tmp;

//  * `KEY` - 32 bytes, the sending exchange's ephemeral public key
//  * `NONCE` - 24 bytes, randomly generated
//  * `CIPHERTEXT` - the inner packet bytes encrypted using secretbox() using the `NONCE` as the nonce and the shared secret (derived from the recipients endpoint key and the included ephemeral key) as the key
//  * `AUTH` - 16 bytes, the calculated onetimeauth(`KEY` + `INNER`, SHA256(`NONCE` + secret)) using the shared secret derived from both endpoint keys, the hashing is to minimize the chance that the same key input is ever used twice

  if(outer->body_len <= (32+24+0+16)) return NULL;
  tmp = lob_new();
  if(!lob_body(tmp,NULL,outer->body_len-(32+24+0+16))) return lob_free(tmp);

  // get the shared secret
  crypto_box_beforenm(secret, outer->body, local->secret);
  memcpy(nonce,outer->body+crypto_box_PUBLICKEYBYTES,crypto_box_NONCEBYTES);

  // decrypt the inner
  crypto_secretbox_open_easy(tmp->body,
    outer->body+crypto_box_PUBLICKEYBYTES+crypto_box_NONCEBYTES,
    outer->body_len,
    nonce,
    secret);

  // load inner packet
  inner = lob_parse(tmp->body,tmp->body_len);
  lob_free(tmp);
  return inner;
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
  uint8_t secret[crypto_box_BEFORENMBYTES];

  if(!remote || !local || !outer) return 1;
  if(outer->head_len != 1 || outer->head[0] != 0x3a) return 2;

  // generate secret and verify
  crypto_box_beforenm(secret, outer->body, local->secret);
  if(crypto_onetimeauth_verify(outer->body+(outer->body_len-crypto_onetimeauth_BYTES),
    outer->body,
    outer->body_len-crypto_onetimeauth_BYTES,
    secret))
  {
    LOG("OTA verify failed");
    lob_free(outer);
    return 1;
  }

  return 0;
}

lob_t remote_encrypt(remote_t remote, local_t local, lob_t inner)
{
  uint8_t secret[crypto_box_BEFORENMBYTES], nonce[crypto_box_NONCEBYTES], csid = 0x3a;
  lob_t outer;
  size_t inner_len;

  outer = lob_new();
  lob_head(outer,&csid,1);
  inner_len = lob_len(inner);
  if(!lob_body(outer,NULL,32+24+inner_len+16)) return lob_free(outer);

  // copy in the ephemeral public key/nonce
  memcpy(outer->body, remote->ekey, crypto_box_PUBLICKEYBYTES);
  randombytes(nonce,crypto_box_NONCEBYTES);
  memcpy(outer->body+32, nonce, crypto_box_NONCEBYTES);

  // get the shared secret to create the nonce+key for the open aes
  crypto_box_beforenm(secret, remote->key, remote->esecret);

  // encrypt the inner
  crypto_secretbox_easy(outer->body+32+24,
    lob_raw(inner),
    inner_len,
    nonce,
    secret);

  // generate secret for hmac
  crypto_box_beforenm(secret, remote->key, local->secret);
  crypto_onetimeauth(outer->body+32+24+inner_len, outer->body, outer->body_len-16, secret);

  return outer;
}

ephemeral_t ephemeral_new(remote_t remote, lob_t outer)
{
  return NULL;
/*
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
  */
}

void ephemeral_free(ephemeral_t ephem)
{
  free(ephem);
}

lob_t ephemeral_encrypt(ephemeral_t ephem, lob_t inner)
{
  return NULL;
  /*
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
  */
}

lob_t ephemeral_decrypt(ephemeral_t ephem, lob_t outer)
{
  return NULL;
  /*
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
  */
}

/*
int crypt_new_3a(crypt_t c, unsigned char *key, int len)
{
  unsigned char hash[32];
  crypt_3a_t cs;
  
  if(!key || len <= 0) return 1;
  
  c->cs = malloc(sizeof(struct crypt_3a_struct));
  memset(c->cs, 0, sizeof (struct crypt_3a_struct));
  cs = (crypt_3a_t)c->cs;

  if(len == crypto_box_PUBLICKEYBYTES)
  {
    memcpy(cs->id_public,key,crypto_box_PUBLICKEYBYTES);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength((char*)key,0) != crypto_box_PUBLICKEYBYTES || base64dec(cs->id_public,(char*)key,0)) return -1;
  }
  
  // create line ephemeral key
  crypto_box_keypair(cs->line_public,cs->line_private);

  // generate fingerprint
  crypto_hash_sha256(hash,cs->id_public,crypto_box_PUBLICKEYBYTES);

  // alloc/copy in the public values (free'd by crypt_free)  
  c->part = malloc((32*2)+1);
  c->keylen = crypto_box_PUBLICKEYBYTES;
  c->key = malloc(c->keylen);
  memcpy(c->key,cs->id_public,crypto_box_PUBLICKEYBYTES);
  util_hex(hash,32,(unsigned char*)c->part);

  return 0;
}


void crypt_free_3a(crypt_t c)
{
  if(c->cs) free(c->cs);
}

int crypt_keygen_3a(lob_t p)
{
  char b64[crypto_box_SECRETKEYBYTES*4];
  uint8_t id_private[crypto_box_SECRETKEYBYTES], id_public[crypto_box_PUBLICKEYBYTES];

  // create identity keypair
  crypto_box_keypair(id_public,id_private);

  base64enc(b64,id_public,crypto_box_PUBLICKEYBYTES);
  lob_set_str(p,"3a",b64);

  base64enc(b64,id_private,crypto_box_SECRETKEYBYTES);
  lob_set_str(p,"3a_secret",b64);

  return 0;
}

int crypt_private_3a(crypt_t c, unsigned char *key, int len)
{
  crypt_3a_t cs = (crypt_3a_t)c->cs;
  
  if(!key || len <= 0) return 1;

  if(len == crypto_box_SECRETKEYBYTES)
  {
    memcpy(cs->id_private,key,crypto_box_SECRETKEYBYTES);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength((char*)key,0) != crypto_box_SECRETKEYBYTES || base64dec(cs->id_private,(char*)key,0)) return -1;
  }

  c->isprivate = 1;
  return 0;
}

lob_t crypt_lineize_3a(crypt_t c, lob_t p)
{
  lob_t line;
  crypt_3a_t cs = (crypt_3a_t)c->cs;

  line = lob_chain(p);
  lob_body(line,NULL,16+crypto_secretbox_NONCEBYTES+lob_len(p)+crypto_secretbox_MACBYTES);
  memcpy(line->body,c->lineIn,16);
  randombytes(line->body+16,crypto_box_NONCEBYTES);

  crypto_secretbox_easy(line->body+16+crypto_secretbox_NONCEBYTES,
    lob_raw(p),
    lob_len(p),
    line->body+16,
    cs->keyOut);
  
  return line;
}

lob_t crypt_delineize_3a(crypt_t c, lob_t p)
{
  lob_t line;
  crypt_3a_t cs = (crypt_3a_t)c->cs;

  crypto_secretbox_open_easy(p->body+16+crypto_secretbox_NONCEBYTES,
    p->body+16+crypto_secretbox_NONCEBYTES,
    p->body_len-(16+crypto_secretbox_NONCEBYTES),
    p->body+16,
    cs->keyIn);
  
  line = lob_parse(p->body+16+crypto_secretbox_NONCEBYTES, p->body_len-(16+crypto_secretbox_NONCEBYTES+crypto_secretbox_MACBYTES));
  lob_free(p);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_3a(crypt_t c, lob_t inner)
{
  unsigned char line_public[crypto_box_PUBLICKEYBYTES], secret[crypto_box_BEFORENMBYTES], input[crypto_box_BEFORENMBYTES+16+16], hash[32];
  char *hecc;
  crypt_3a_t cs;
  
  cs = (crypt_3a_t)c->cs;
  hecc = lob_get_str(inner,"ecc"); // it's where we stashed it
  if(!hecc || strlen(hecc) != crypto_box_PUBLICKEYBYTES*2) return 1;

  // do the diffie hellman
  util_unhex((unsigned char*)hecc,crypto_box_PUBLICKEYBYTES*2,line_public);
  crypto_box_beforenm(secret, line_public, cs->line_private);

  // make line keys!
  memcpy(input,secret,crypto_box_BEFORENMBYTES);
  memcpy(input+crypto_box_BEFORENMBYTES,c->lineOut,16);
  memcpy(input+crypto_box_BEFORENMBYTES+16,c->lineIn,16);
  crypto_hash_sha256(hash,input,crypto_box_BEFORENMBYTES+16+16);
  memcpy(cs->keyOut,hash,32);

  memcpy(input+crypto_box_BEFORENMBYTES,c->lineIn,16);
  memcpy(input+crypto_box_BEFORENMBYTES+16,c->lineOut,16);
  crypto_hash_sha256(hash,input,crypto_box_BEFORENMBYTES+16+16);
  memcpy(cs->keyIn,hash,32);

  return 0;
}

// create a new open packet
lob_t crypt_openize_3a(crypt_t self, crypt_t c, lob_t inner)
{
  unsigned char secret[crypto_box_BEFORENMBYTES], iv[crypto_box_NONCEBYTES];
  lob_t open;
  int inner_len;
  crypt_3a_t cs = (crypt_3a_t)c->cs, scs = (crypt_3a_t)self->cs;

  open = lob_chain(inner);
  lob_json(open,&(self->csid),1);
  inner_len = lob_len(inner);
  lob_body(open,NULL,crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES+inner_len+crypto_secretbox_MACBYTES);

  // copy in the line public key
  memcpy(open->body+crypto_onetimeauth_BYTES, cs->line_public, crypto_box_PUBLICKEYBYTES);

  // get the shared secret to create the iv+key for the open aes
  crypto_box_beforenm(secret,cs->id_public, cs->line_private);
  memset(iv,0,crypto_box_NONCEBYTES);
  iv[crypto_box_NONCEBYTES-1] = 1;

  // encrypt the inner
  crypto_secretbox_easy(open->body+crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES,
    lob_raw(inner),
    inner_len,
    iv,
    secret);

  // generate secret for hmac
  crypto_box_beforenm(secret, cs->id_public, scs->id_private);
  crypto_onetimeauth(open->body,open->body+crypto_onetimeauth_BYTES,crypto_box_PUBLICKEYBYTES+inner_len+crypto_secretbox_MACBYTES,secret);

  return open;
}

lob_t crypt_deopenize_3a(crypt_t self, lob_t open)
{
  unsigned char secret[crypto_box_BEFORENMBYTES], iv[crypto_box_NONCEBYTES], ecc[crypto_box_PUBLICKEYBYTES*2+1];
  lob_t inner, tmp;
  crypt_3a_t cs = (crypt_3a_t)self->cs;

//  unsigned char buf[1024];
//  DEBUG_PRINTF("iv %s",util_hex(iv,crypto_box_NONCEBYTES,buf));
//  DEBUG_PRINTF("secret %s",util_hex(secret,crypto_box_BEFORENMBYTES,buf));

  if(open->body_len <= (crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES)) return NULL;
  inner = lob_new();
  lob_body(inner,NULL,open->body_len-(crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES));

  // get the shared secret to create the iv+key for the open aes
  crypto_box_beforenm(secret, open->body+crypto_onetimeauth_BYTES, cs->id_private);
  memset(iv,0,crypto_box_NONCEBYTES);
  iv[crypto_box_NONCEBYTES-1] = 1;

  // decrypt the inner
  crypto_secretbox_open_easy(inner->body,
    open->body+crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES,
    inner->body_len,
    iv,
    secret);

  // load inner packet
  if((tmp = lob_parse(inner->body,inner->body_len-crypto_secretbox_MACBYTES)) == NULL) return lob_free(inner);
  lob_free(inner);
  inner = tmp;

  // generate secret and verify
  if(inner->body_len != crypto_box_PUBLICKEYBYTES) return lob_free(inner);
  crypto_box_beforenm(secret, inner->body, cs->id_private);
  if(crypto_onetimeauth_verify(open->body,
    open->body+crypto_onetimeauth_BYTES,
    open->body_len-crypto_onetimeauth_BYTES,
    secret)) return lob_free(inner);

  // stash the hex line key w/ the inner
  util_hex(open->body+crypto_onetimeauth_BYTES,crypto_box_PUBLICKEYBYTES,ecc);
  lob_set_str(inner,"ecc",(char*)ecc);

  return inner;
}
*/
