#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sodium.h>
#include "switch.h"
#include "base64_enc.h"
#include "base64_dec.h"

typedef struct crypt_3a_struct
{
  uint8_t id_private[32], id_public[32 *2], line_private[32], line_public[32 *2];
  uint32_t seq;
  unsigned char keyOut[16], keyIn[16];
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

  if(len == 32*2)
  {
    memcpy(cs->id_public,key,32*2);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength((char*)key,0) != 32*2 || base64dec(cs->id_public,(char*)key,0)) return -1;
  }
  
  // generate fingerprint
  crypto_hash_sha256(hash,cs->id_public,32*2);

  sha1(cs->id_public,32*2,hash);

  // create line ephemeral key
  var kp = sodium.crypto_box_keypair();

  uECC_make_key(cs->line_public, cs->line_private);

  // alloc/copy in the public values (free'd by crypt_free)  
  c->part = malloc(32*2+1);
  c->keylen = 32*2;
  c->key = malloc(c->keylen);
  memcpy(c->key,cs->id_public,32*2);
  util_hex(hash,32,(unsigned char*)c->part);

  return 0;
}


void crypt_free_3a(crypt_t c)
{
  if(c->cs) free(c->cs);
}

int crypt_keygen_3a(packet_t p)
{
  char b64[32*4];
  uint8_t id_private[32], id_public[32*2];

  // create line ephemeral key
  uECC_make_key(id_public, id_private);

  base64enc(b64,id_public,32*2);
  packet_set_str(p,"1a",b64);

  base64enc(b64,id_private,32);
  packet_set_str(p,"1a_secret",b64);

  return 0;
}

int crypt_private_3a(crypt_t c, unsigned char *key, int len)
{
  crypt_3a_t cs = (crypt_3a_t)c->cs;
  
  if(!key || len <= 0) return 1;

  if(len == 32)
  {
    memcpy(cs->id_private,key,32);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength((char*)key,0) != 32 || base64dec(cs->id_private,(char*)key,0)) return -1;
  }

  c->isprivate = 1;
  return 0;
}

packet_t crypt_lineize_3a(crypt_t c, packet_t p)
{
  packet_t line;
  aes_context ctx;
  unsigned char iv[16], block[16], hmac[32];
  size_t off = 0;
  crypt_3a_t cs = (crypt_3a_t)c->cs;

  line = packet_chain(p);
  packet_body(line,NULL,16+4+4+packet_len(p));
  memcpy(line->body,c->lineIn,16);
  memcpy(line->body+16+4,&(cs->seq),4);
  memset(iv,0,16);
  memcpy(iv+12,&(cs->seq),4);
  cs->seq++;

  aes_setkey_enc(&ctx,cs->keyOut,128);
  aes_crypt_ctr(&ctx,packet_len(p),&off,iv,block,packet_raw(p),line->body+16+4+4);

  sha1_hmac(cs->keyOut,16,line->body+16+4,4+packet_len(p),hmac);
  memcpy(line->body+16,hmac,4);

  return line;
}

packet_t crypt_delineize_3a(crypt_t c, packet_t p)
{
  packet_t line;
  aes_context ctx;
  unsigned char block[16], iv[16], hmac[32];
  size_t off = 0;
  crypt_3a_t cs = (crypt_3a_t)c->cs;

  memset(iv,0,16);
  memcpy(iv+12,p->body+16+4,4);

  sha1_hmac(cs->keyIn,16,p->body+16+4,p->body_len-(16+4),hmac);
  if(memcmp(hmac,p->body+16,4) != 0) return packet_free(p);

  aes_setkey_enc(&ctx,cs->keyIn,128);
  aes_crypt_ctr(&ctx,p->body_len-(16+4+4),&off,iv,block,p->body+16+4+4,p->body+16+4+4);

  line = packet_parse(p->body+16+4+4, p->body_len-(16+4+4));
  packet_free(p);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_3a(crypt_t c, packet_t inner)
{
  unsigned char line_public[32*2], secret[32], input[32+16+16], hash[32];
  char *hecc;
  crypt_3a_t cs;
  
  cs = (crypt_3a_t)c->cs;
  hecc = packet_get_str(inner,"ecc"); // it's where we stashed it
  if(!hecc || strlen(hecc) != 32*4) return 1;

  // do the diffie hellman
  util_unhex((unsigned char*)hecc,32*4,line_public);
  if(!uECC_shared_secret(line_public, cs->line_private, secret)) return 1;

  // make line keys!
  memcpy(input,secret,32);
  memcpy(input+32,c->lineOut,16);
  memcpy(input+32+16,c->lineIn,16);
  sha1(input,32+16+16,hash);
  memcpy(cs->keyOut,hash,16);

  memcpy(input+32,c->lineIn,16);
  memcpy(input+32+16,c->lineOut,16);
  sha1(input,32+16+16,hash);
  memcpy(cs->keyIn,hash,16);

  return 0;
}

// create a new open packet
packet_t crypt_openize_3a(crypt_t self, crypt_t c, packet_t inner)
{
  unsigned char secret[32], iv[16], block[16];
  packet_t open;
  aes_context ctx;
  int inner_len;
  size_t off = 0;
  crypt_3a_t cs = (crypt_3a_t)c->cs, scs = (crypt_3a_t)self->cs;

  open = packet_chain(inner);
  packet_json(open,&(self->csid),1);
  inner_len = packet_len(inner);
  packet_body(open,NULL,32+40+inner_len);

  // copy in the line public key
  memcpy(open->body+32, cs->line_public, 40);

  // get the shared secret to create the iv+key for the open aes
  if(!uECC_shared_secret(cs->id_public, cs->line_private, secret)) return packet_free(open);
  memset(iv,0,16);
  iv[15] = 1;

  // encrypt the inner
  aes_setkey_enc(&ctx,secret,128);
  aes_crypt_ctr(&ctx,inner_len,&off,iv,block,packet_raw(inner),open->body+32+40);

  // generate secret for hmac
  if(!uECC_shared_secret(cs->id_public, scs->id_private, secret)) return packet_free(open);
  sha1_hmac(secret,32,open->body+32,40+inner_len,open->body);

  return open;
}

packet_t crypt_deopenize_3a(crypt_t self, packet_t open)
{
  unsigned char secret[32], iv[16], block[16], b64[32*2*2], hmac[32];
  aes_context ctx;
  packet_t inner, tmp;
  size_t off = 0;
  crypt_3a_t cs = (crypt_3a_t)self->cs;

  if(open->body_len <= (32+40)) return NULL;
  inner = packet_new();
  packet_body(inner,NULL,open->body_len-(32+40));

  // get the shared secret to create the iv+key for the open aes
  if(!uECC_shared_secret(open->body+32, cs->id_private, secret)) return packet_free(inner);
  memset(iv,0,16);
  iv[15] = 1;

  // decrypt the inner
  aes_setkey_enc(&ctx,secret,128); // tricky, must use _enc to decrypt here
  aes_crypt_ctr(&ctx,inner->body_len,&off,iv,block,open->body+32+40,inner->body);

  // load inner packet
  if((tmp = packet_parse(inner->body,inner->body_len)) == NULL) return packet_free(inner);
  packet_free(inner);
  inner = tmp;

  // generate secret for hmac
  if(inner->body_len != 32*2) return packet_free(inner);
  if(!uECC_shared_secret(inner->body, cs->id_private, secret)) return packet_free(inner);

  // verify
  sha1_hmac(secret,32,open->body+32,open->body_len-32,hmac);
  if(memcmp(hmac,open->body,32) != 0) return packet_free(inner);

  // stash the hex line key w/ the inner
  util_hex(open->body+32,40,b64);
  packet_set_str(inner,"ecc",(char*)b64);

  return inner;
}

