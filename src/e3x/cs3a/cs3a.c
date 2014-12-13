#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sodium.h>

e3x_cipher_t cs3a_init(lob_t options)
{
  e3x_cipher_t ret = malloc(sizeof(struct e3x_cipher_struct));
  if(!ret) return NULL;
  memset(ret,0,sizeof (struct e3x_cipher_struct));
  
  // identifying markers
  ret->id = CS_3a;
  ret->csid = 0x3a;
  memcpy(ret->hex,"3a",3);
  
  // TODO
  return ret;
}

/*
unsigned char *crypt_rand(unsigned char *s, int len)
{
  randombytes_buf((void * const)s, (const size_t)len);
  return s;
}

unsigned char *crypt_hash(unsigned char *input, unsigned long len, unsigned char *output)
{
  crypto_hash_sha256(output,input,(unsigned long)len);
  return output;
}

char *crypt_err()
{
  return 0;
}


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
