#include "avr.h"
#include "aes.h"

typedef struct crypt_1a_struct
{
  uint8_t id_private[uECC_BYTES], id_public[uECC_BYTES *2], line_private[uECC_BYTES], line_public[uECC_BYTES *2];
  uint32_t seq;
  unsigned char keyOut[16], keyIn[16];
} *crypt_1a_t;

/*
void aes_ctr(void *key, void *iv, void *in, void *out, uint32_t len)
{
  bcal_ctr_ctx_t ctx;
  DEBUG_PRINTF("aes_ctr of %d %d %d %d %d",sizeof(key),sizeof(iv),sizeof(in),sizeof(out),len);
//  bcal_ctr_init(&aes128_desc, key, 128, NULL, &ctx);
  DEBUG_PRINTF("init");
//  bcal_ctr_encMsg(iv, in, len, &ctx);
  DEBUG_PRINTF("enc");
//  bcal_ctr_free(&ctx);
  memcpy(out,in,len);
  DEBUG_PRINTF("ctr done");
}
*/

int RNG(uint8_t *p_dest, unsigned p_size)
{
  while(p_size--)
  {
    *p_dest = (uint8_t)random();
    p_dest++;
  }
  return 1;
}

int crypt_init_1a()
{
  DEBUG_PRINTF("CS1a init");
  uECC_set_rng(&RNG);
  return 0;
}

int crypt_new_1a(crypt_t c, unsigned char *key, int len)
{
  unsigned char hash[20];
  crypt_1a_t cs;
  
  if(!key || len <= 0) return 1;
  
  c->cs = malloc(sizeof(struct crypt_1a_struct));
  memset(c->cs, 0, sizeof (struct crypt_1a_struct));
  cs = (crypt_1a_t)c->cs;

  if(len == uECC_BYTES*2)
  {
    memcpy(cs->id_public,key,uECC_BYTES*2);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength((char*)key,0) != uECC_BYTES*2 || base64dec(cs->id_public,(char*)key,0)) return -1;
  }
  
  // generate fingerprint
  sha1(hash,cs->id_public,uECC_BYTES*2*8);

  // create line ephemeral key
  uECC_make_key(cs->line_public, cs->line_private);

  // alloc/copy in the public values (free'd by crypt_free)  
  c->part = malloc(20*2+1);
  c->keylen = uECC_BYTES*2;
  c->key = malloc(c->keylen);
  memcpy(c->key,cs->id_public,uECC_BYTES*2);
  util_hex(hash,20,(unsigned char*)c->part);

  return 0;
}


void crypt_free_1a(crypt_t c)
{
  if(c->cs) free(c->cs);
}

int crypt_keygen_1a(packet_t p)
{
  char b64[uECC_BYTES*4];
  uint8_t id_private[uECC_BYTES], id_public[uECC_BYTES*2];

  // create line ephemeral key
  uECC_make_key(id_public, id_private);

  base64enc(b64,id_public,uECC_BYTES*2);
  packet_set_str(p,"1a",b64);

  base64enc(b64,id_private,uECC_BYTES);
  packet_set_str(p,"1a_secret",b64);

  return 0;
}

int crypt_private_1a(crypt_t c, unsigned char *key, int len)
{
  crypt_1a_t cs = (crypt_1a_t)c->cs;
  
  if(!key || len <= 0) return 1;

  if(len == uECC_BYTES)
  {
    memcpy(cs->id_private,key,uECC_BYTES);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength((char*)key,0) != uECC_BYTES || base64dec(cs->id_private,(char*)key,0)) return -1;
  }

  c->isprivate = 1;
  return 0;
}

packet_t crypt_lineize_1a(crypt_t c, packet_t p)
{
  packet_t line;
  unsigned char iv[16], block[16], hmac[20];
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)c->cs;
  aes_context ctx;

  line = packet_chain(p);
  packet_body(line,NULL,16+4+4+packet_len(p));
  memcpy(line->body,c->lineIn,16);
  memcpy(line->body+16+4,&(cs->seq),4);
  memset(iv,0,16);
  memcpy(iv+12,&(cs->seq),4);
  cs->seq++;

  aes_setkey_enc(&ctx,cs->keyOut,128);
  aes_crypt_ctr(&ctx,packet_len(p),&off,iv,block,packet_raw(p),line->body+16+4+4);

  hmac_sha1(hmac,cs->keyOut,16*8,line->body+16+4,(4+packet_len(p))*8);
  memcpy(line->body+16,hmac,4);

  return line;
}

packet_t crypt_delineize_1a(crypt_t c, packet_t p)
{
  packet_t line;
  unsigned char block[16], iv[16], hmac[20];
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)c->cs;
  aes_context ctx;

  memset(iv,0,16);
  memcpy(iv+12,p->body+16+4,4);

  aes_setkey_enc(&ctx,cs->keyIn,128);
  aes_crypt_ctr(&ctx,p->body_len-(16+4+4),&off,iv,block,p->body+16+4+4,p->body+16+4+4);

  hmac_sha1(hmac,cs->keyIn,16*8,p->body+16+4,(p->body_len-(16+4))*8);
  if(memcmp(hmac,p->body+16,4) != 0) return packet_free(p);

  line = packet_parse(p->body+16+4+4, p->body_len-(16+4+4));
  packet_free(p);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_1a(crypt_t c, packet_t inner)
{
  unsigned char line_public[uECC_BYTES*2], secret[uECC_BYTES], input[uECC_BYTES+16+16], hash[20];
  char *hecc;
  crypt_1a_t cs;
  
  cs = (crypt_1a_t)c->cs;
  hecc = packet_get_str(inner,"ecc"); // it's where we stashed it
  if(!hecc || strlen(hecc) != uECC_BYTES*4) return 1;

  // do the diffie hellman
  util_unhex((unsigned char*)hecc,uECC_BYTES*4,line_public);
  if(!uECC_shared_secret(line_public, cs->line_private, secret)) return 1;

  // make line keys!
  memcpy(input,secret,uECC_BYTES);
  memcpy(input+uECC_BYTES,c->lineOut,16);
  memcpy(input+uECC_BYTES+16,c->lineIn,16);
  sha1(hash,input,(uECC_BYTES+16+16)*8);
  memcpy(cs->keyOut,hash,16);

  memcpy(input+uECC_BYTES,c->lineIn,16);
  memcpy(input+uECC_BYTES+16,c->lineOut,16);
  sha1(hash,input,(uECC_BYTES+16+16)*8);
  memcpy(cs->keyIn,hash,16);

  return 0;
}

// create a new open packet
packet_t crypt_openize_1a(crypt_t self, crypt_t c, packet_t inner)
{
  unsigned char secret[uECC_BYTES], iv[16], block[16];
  packet_t open;
  int inner_len;
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)c->cs, scs = (crypt_1a_t)self->cs;
  aes_context ctx;

  open = packet_chain(inner);
  packet_json(open,&(self->csid),1);
  inner_len = packet_len(inner);
  packet_body(open,NULL,20+40+inner_len);

  // copy in the line public key
  memcpy(open->body+20, cs->line_public, 40);

  // get the shared secret to create the iv+key for the open aes
  if(!uECC_shared_secret(cs->id_public, cs->line_private, secret)) return packet_free(open);
  memset(iv,0,16);
  iv[15] = 1;

  // encrypt the inner
  aes_setkey_enc(&ctx,secret,128);
  aes_crypt_ctr(&ctx,inner_len,&off,iv,block,packet_raw(inner),open->body+20+40);

  // generate secret for hmac
  if(!uECC_shared_secret(cs->id_public, scs->id_private, secret)) return packet_free(open);
  hmac_sha1(open->body,secret,uECC_BYTES*8,open->body+20,(40+inner_len)*8);

  return open;
}

packet_t crypt_deopenize_1a(crypt_t self, packet_t open)
{
  unsigned char secret[uECC_BYTES], iv[16], block[16], b64[uECC_BYTES*2*2], hmac[20];
  packet_t inner, tmp;
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)self->cs;
  aes_context ctx;

  if(open->body_len <= (20+40)) return NULL;
  inner = packet_new();
  packet_body(inner,NULL,open->body_len-(20+40));

  // get the shared secret to create the iv+key for the open aes
  if(!uECC_shared_secret(open->body+20, cs->id_private, secret)) return packet_free(inner);
  memset(iv,0,16);
  iv[15] = 1;

  // decrypt the inner
  aes_setkey_enc(&ctx,secret,128); // tricky, must use _enc to decrypt here
  aes_crypt_ctr(&ctx,inner->body_len,&off,iv,block,open->body+20+40,inner->body);

  // load inner packet
  if((tmp = packet_parse(inner->body,inner->body_len)) == NULL) return packet_free(inner);
  packet_free(inner);
  inner = tmp;

  // generate secret for hmac
  if(inner->body_len != uECC_BYTES*2) return packet_free(inner);
  if(!uECC_shared_secret(inner->body, cs->id_private, secret)) return packet_free(inner);

  // verify
  hmac_sha1(hmac,secret,uECC_BYTES*8,open->body+20,(open->body_len-20)*8);
  if(memcmp(hmac,open->body,20) != 0) return packet_free(inner);

  // stash the hex line key w/ the inner
  util_hex(open->body+20,40,b64);
  packet_set_str(inner,"ecc",(char*)b64);

  return inner;
}

