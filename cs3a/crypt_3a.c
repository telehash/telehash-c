#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sodium.h>
#include "switch.h"
#include "base64_enc.h"
#include "base64_dec.h"

typedef struct crypt_3a_struct
{
  uint8_t id_private[crypto_box_SECRETKEYBYTES], id_public[crypto_box_PUBLICKEYBYTES], line_private[crypto_box_SECRETKEYBYTES], line_public[crypto_box_PUBLICKEYBYTES];
  unsigned char keyOut[32], keyIn[32];
} *crypt_3a_t;


int crypt_init_3a()
{
  randombytes_stir();
  return 0;
}

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

int crypt_keygen_3a(packet_t p)
{
  char b64[crypto_box_SECRETKEYBYTES*4];
  uint8_t id_private[crypto_box_SECRETKEYBYTES], id_public[crypto_box_PUBLICKEYBYTES];

  // create identity keypair
  crypto_box_keypair(id_public,id_private);

  base64enc(b64,id_public,crypto_box_PUBLICKEYBYTES);
  packet_set_str(p,"3a",b64);

  base64enc(b64,id_private,crypto_box_SECRETKEYBYTES);
  packet_set_str(p,"3a_secret",b64);

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

packet_t crypt_lineize_3a(crypt_t c, packet_t p)
{
  packet_t line;
  crypt_3a_t cs = (crypt_3a_t)c->cs;

  line = packet_chain(p);
  packet_body(line,NULL,16+crypto_secretbox_NONCEBYTES+packet_len(p));
  memcpy(line->body,c->lineIn,16);

  crypto_secretbox(line->body+16+crypto_secretbox_NONCEBYTES,
    packet_raw(p),
    packet_len(p),
    line->body+16,
    cs->keyOut);

  return line;
}

packet_t crypt_delineize_3a(crypt_t c, packet_t p)
{
  packet_t line;
  crypt_3a_t cs = (crypt_3a_t)c->cs;

  crypto_secretbox_open(p->body+16+crypto_secretbox_NONCEBYTES,
    p->body+16+crypto_secretbox_NONCEBYTES,
    p->body_len-(16+crypto_secretbox_NONCEBYTES),
    p->body+16,
    cs->keyIn);
  
  line = packet_parse(p->body+16+crypto_secretbox_NONCEBYTES, p->body_len-(16+crypto_secretbox_NONCEBYTES));
  packet_free(p);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_3a(crypt_t c, packet_t inner)
{
  unsigned char line_public[crypto_box_PUBLICKEYBYTES], secret[crypto_box_BEFORENMBYTES], input[crypto_box_BEFORENMBYTES+16+16], hash[32];
  char *hecc;
  crypt_3a_t cs;
  
  cs = (crypt_3a_t)c->cs;
  hecc = packet_get_str(inner,"ecc"); // it's where we stashed it
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
packet_t crypt_openize_3a(crypt_t self, crypt_t c, packet_t inner)
{
  unsigned char secret[crypto_box_BEFORENMBYTES], iv[crypto_box_NONCEBYTES];
  packet_t open;
  int inner_len;
  crypt_3a_t cs = (crypt_3a_t)c->cs, scs = (crypt_3a_t)self->cs;

  open = packet_chain(inner);
  packet_json(open,&(self->csid),1);
  inner_len = packet_len(inner);
  packet_body(open,NULL,crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES+inner_len);

  // copy in the line public key
  memcpy(open->body+crypto_onetimeauth_BYTES, cs->line_public, crypto_box_PUBLICKEYBYTES);

  // get the shared secret to create the iv+key for the open aes
  crypto_box_beforenm(secret,cs->id_public, cs->line_private);
  memset(iv,0,crypto_box_NONCEBYTES);
  iv[crypto_box_NONCEBYTES-1] = 1;

  // encrypt the inner
  crypto_secretbox(open->body+crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES,
    packet_raw(inner),
    inner_len,
    iv,
    secret);

  // generate secret for hmac
  crypto_box_beforenm(secret, cs->id_public, scs->id_private);
  crypto_onetimeauth(open->body,open->body+crypto_onetimeauth_BYTES,crypto_onetimeauth_BYTES+inner_len,secret);

  return open;
}

packet_t crypt_deopenize_3a(crypt_t self, packet_t open)
{
  unsigned char secret[crypto_box_BEFORENMBYTES], iv[crypto_box_NONCEBYTES], ecc[crypto_box_PUBLICKEYBYTES*2+1];
  packet_t inner, tmp;
  crypt_3a_t cs = (crypt_3a_t)self->cs;

  if(open->body_len <= (crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES)) return NULL;
  inner = packet_new();
  packet_body(inner,NULL,open->body_len-(crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES));

  // get the shared secret to create the iv+key for the open aes
  crypto_box_beforenm(secret, open->body+crypto_onetimeauth_BYTES, cs->id_private);
  memset(iv,0,crypto_box_NONCEBYTES);
  iv[crypto_box_NONCEBYTES-1] = 1;

  // decrypt the inner
  crypto_secretbox_open(inner->body,
    open->body+crypto_onetimeauth_BYTES+crypto_box_PUBLICKEYBYTES,
    inner->body_len,
    iv,
    secret);

  // load inner packet
  if((tmp = packet_parse(inner->body,inner->body_len)) == NULL) return packet_free(inner);
  packet_free(inner);
  inner = tmp;

  // generate secret and verify
  if(inner->body_len != crypto_box_PUBLICKEYBYTES) return packet_free(inner);
  crypto_box_beforenm(secret, inner->body, cs->id_private);
  if(crypto_onetimeauth_verify(open->body,
    open->body+crypto_onetimeauth_BYTES,
    open->body_len-crypto_onetimeauth_BYTES,
    secret)) return packet_free(inner);

  // stash the hex line key w/ the inner
  util_hex(open->body+crypto_onetimeauth_BYTES,crypto_box_PUBLICKEYBYTES,ecc);
  packet_set_str(inner,"ecc",(char*)ecc);

  return inner;
}

