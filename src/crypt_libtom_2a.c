#include "crypt_libtom.h"

typedef struct crypt_libtom_struct
{
  rsa_key rsa;
  ecc_key eccOut;
  unsigned char keyOut[32], keyIn[32];
} *crypt_libtom_t;

int crypt_init_2a()
{
  return crypt_libtom_init();
}

int crypt_new_2a(crypt_t c, unsigned char *key, int len)
{
  int der_len = 512;
  unsigned long fplen=32;
  unsigned char der[der_len], fp[32];
  crypt_libtom_t cs;
  
  if(!key || !len || len > der_len) return 1;
  
  c->cs = malloc(sizeof(struct crypt_libtom_struct));
  bzero(c->cs, sizeof (struct crypt_libtom_struct));
  cs = (crypt_libtom_t)c->cs;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, (unsigned long*)&der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  // try to load rsa public key
  if((_crypt_libtom_err = rsa_import(der, der_len, &(cs->rsa))) != CRYPT_OK) return 1;

  // re-export it to be safe
  if((_crypt_libtom_err = rsa_export(der, (unsigned long*)&der_len, PK_PUBLIC, &(cs->rsa))) != CRYPT_OK) return 1;
  
  // generate fingerprint
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),der,der_len,fp,&fplen)) != CRYPT_OK) return 1;

  // create line ephemeral key
  if((_crypt_libtom_err = ecc_make_key(&_crypt_libtom_prng, find_prng("yarrow"), 32, &(cs->eccOut))) != CRYPT_OK) return 1;

  // alloc/copy in the public values (free'd by crypt_free)  
  c->part = malloc(65);
  c->keylen = der_len;
  c->key = malloc(c->keylen);
  memcpy(c->key,der,der_len);
  util_hex(fp,32,(unsigned char*)c->part);

  return 0;
}


void crypt_free_2a(crypt_t c)
{
  if(c->cs) free(c->cs);
}

int crypt_keygen_2a(packet_t p)
{
  unsigned long len, len2;
  unsigned char *buf, *b64;
  rsa_key rsa;

  if ((_crypt_libtom_err = rsa_make_key(&_crypt_libtom_prng, find_prng("yarrow"), 256, 65537, &rsa)) != CRYPT_OK) return -1;

  // this determines the size needed into len;
  len = 1;
  buf = malloc(len);
  rsa_export(buf, &len, PK_PRIVATE, &rsa);
  buf = realloc(buf,len);
  len2 = len*1.4;
  b64 = malloc(len2);

  // export/base64 private
  if((_crypt_libtom_err = rsa_export(buf, &len, PK_PRIVATE, &rsa)) != CRYPT_OK)
  {
    free(buf);
    free(b64);
    return -1;
  }
  if((_crypt_libtom_err = base64_encode(buf, len, b64, &len2)) != CRYPT_OK) {
    free(buf);
    free(b64);
    return -1;
  }
  b64[len2] = 0;
  packet_set_str(p,"2a_",(char*)b64);

  if((_crypt_libtom_err = rsa_export(buf, &len, PK_PUBLIC, &rsa)) != CRYPT_OK)
  {
    free(buf);
    free(b64);
    return -1;
  }
  if((_crypt_libtom_err = base64_encode(buf, len, b64, &len2)) != CRYPT_OK) {
    free(buf);
    free(b64);
    return -1;
  }
  b64[len2] = 0;
  packet_set_str(p,"2a",(char*)b64);
  return 0;
}

int crypt_private_2a(crypt_t c, unsigned char *key, int len)
{
  unsigned long der_len = 4096;
  unsigned char der[der_len];
  crypt_libtom_t cs = (crypt_libtom_t)c->cs;
  
  if(len > der_len) return 1;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, &der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  if((_crypt_libtom_err = rsa_import(der, der_len, &(cs->rsa))) != CRYPT_OK) return 1;
  c->private = 1;
  return 0;
}

packet_t crypt_lineize_2a(crypt_t c, packet_t p)
{
  packet_t line;
  unsigned char iv[16], hex[33], *enc;
  symmetric_CTR ctr;
  crypt_libtom_t cs = (crypt_libtom_t)c->cs;

  line = packet_chain(p);
  crypt_rand(iv,16);
  packet_set_str(line,"type","line");
  packet_set_str(line,"iv", (char*)util_hex(iv,16,hex));
  packet_set_str(line,"line", (char*)util_hex(c->lineIn,16,hex));

  // create aes cipher now and encrypt
  if((_crypt_libtom_err = ctr_start(find_cipher("aes"), iv, cs->keyOut, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(line);
  enc = malloc(packet_len(p));
  if((_crypt_libtom_err = ctr_encrypt(packet_raw(p),enc,packet_len(p),&ctr)) != CRYPT_OK)
  {
    free(enc);
    return packet_free(line);
  }
  packet_body(line,enc,packet_len(p));
  free(enc);

  return line;
}

packet_t crypt_delineize_2a(crypt_t c, packet_t p)
{
  packet_t line;
  unsigned char iv[16], *dec;
  char *hiv;
  symmetric_CTR ctr;
  crypt_libtom_t cs = (crypt_libtom_t)c->cs;

  hiv = packet_get_str(p, "iv");
  if(strlen(hiv) != 32) return packet_free(p);
  util_unhex((unsigned char*)hiv,32,iv);

  // create aes cipher now and encrypt
  if((_crypt_libtom_err = ctr_start(find_cipher("aes"), iv, cs->keyIn, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(p);
  dec = malloc(p->body_len);
  if((_crypt_libtom_err = ctr_decrypt(p->body,dec,p->body_len,&ctr)) != CRYPT_OK)
  {
    free(dec);
    return packet_free(p);
  }
  line = packet_parse(dec, p->body_len);
  packet_free(p);
  free(dec);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_2a(crypt_t c, packet_t inner)
{
  unsigned char ecc[65], secret[32], input[64];
  char *hline, *hecc;
  unsigned long at, len;
  crypt_libtom_t cs;
  ecc_key eccIn;
  
  if(!c || !inner) return 1;
  cs = (crypt_libtom_t)c->cs;
  at = strtol(packet_get_str(inner,"at"), NULL, 10);
  hline = packet_get_str(inner,"line");
  hecc = packet_get_str(inner,"ecc"); // it's where we stashed it
  if(at <= 0 || at <= c->atIn || strlen(hline) != 32 || strlen(hecc) != 128) return 1;

  // do the diffie hellman
  ecc[0] = 0x04; // make it the "uncompressed" format
  util_unhex((unsigned char*)hecc,128,ecc+1);
  if((_crypt_libtom_err = ecc_ansi_x963_import(ecc, 65, &eccIn)) != CRYPT_OK) return 1;
  len = sizeof(secret);
  if((_crypt_libtom_err = ecc_shared_secret(&(cs->eccOut),&eccIn,secret,&len)) != CRYPT_OK) return 1;

  // make line keys!
  memcpy(input,secret,32);
  memcpy(input+32,c->lineOut,16);
  memcpy(input+48,c->lineIn,16);
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"), input, 64, cs->keyOut, &len)) != CRYPT_OK) return 1;
  memcpy(input,secret,32);
  memcpy(input+32,c->lineIn,16);
  memcpy(input+48,c->lineOut,16);
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"), input, 64, cs->keyIn, &len)) != CRYPT_OK) return 1;

  c->atIn = at;
  c->lined = 1;
  return 0;
}

// create a new open packet
packet_t crypt_openize_2a(crypt_t self, crypt_t c, packet_t inner)
{
  unsigned char key[32], iv[16], sig[256], buf[260], upub[65], *pub;
  packet_t open;
  unsigned long len, inner_len;
  gcm_state gcm;
  crypt_libtom_t cs = (crypt_libtom_t)c->cs, scs = (crypt_libtom_t)self->cs;

  open = packet_chain(inner);
  packet_set_str(open,"type","open");
  inner_len = packet_len(inner);
  packet_body(open,NULL,256+260+inner_len+16);

  // create the aes iv and key from ecc
  util_unhex((unsigned char*)"00000000000000000000000000000001",32,iv);
  len = sizeof(upub);
  if((_crypt_libtom_err = ecc_ansi_x963_export(&(cs->eccOut), upub, &len)) != CRYPT_OK) return packet_free(open);
  pub = upub+1; // take off the 0x04 prefix
  len = sizeof(key);
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"), pub, 64, key, &len)) != CRYPT_OK) return packet_free(open);

  // create aes cipher now and encrypt the inner
  len = 16;
//  if((_crypt_libtom_err = gcm_memory(find_cipher("aes"), key, 32, iv, 16, NULL, 0, packet_raw(inner), inner_len, open->body+256+260, open->body+256+260+inner_len, &len, GCM_ENCRYPT)) != CRYPT_OK) return packet_free(open);
  if((_crypt_libtom_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return packet_free(open);
  if((_crypt_libtom_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return packet_free(open);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_libtom_err = gcm_process(&gcm,packet_raw(inner),inner_len,open->body+256+260,GCM_ENCRYPT)) != CRYPT_OK) return packet_free(open);
  if((_crypt_libtom_err = gcm_done(&gcm, open->body+256+260+inner_len, &len)) != CRYPT_OK) return packet_free(open);

  // encrypt the ecc key and attach it
  len = sizeof(buf);
  if((_crypt_libtom_err = rsa_encrypt_key(pub, 64, open->body, &len, 0, 0, &_crypt_libtom_prng, find_prng("yarrow"), find_hash("sha1"), &(cs->rsa))) != CRYPT_OK) return packet_free(open);

  // sign the inner packet
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),open->body+256+260,inner_len+16,key,&len)) != CRYPT_OK) return packet_free(open);
  len = sizeof(sig);
  if((_crypt_libtom_err = rsa_sign_hash_ex(key, 32, sig, &len, LTC_PKCS_1_V1_5, &_crypt_libtom_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(scs->rsa))) != CRYPT_OK) return packet_free(open);

  char shex[2024];
  printf("KHEX %s\n",util_hex(key,32,shex));
  printf("SHEX %d %s\n",len,util_hex(sig,len,shex));

	// encrypt the signature, create the new aes key+cipher first
  memcpy(buf,pub,64);
  memcpy(buf+64,c->lineOut,16);
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),buf,64+16,key,&len)) != CRYPT_OK) return packet_free(open);
  len = 4;
  if((_crypt_libtom_err = gcm_init(&gcm, find_cipher("aes"), key, 32)) != CRYPT_OK) return packet_free(open);
  if((_crypt_libtom_err = gcm_add_iv(&gcm, iv, 16)) != CRYPT_OK) return packet_free(open);
  gcm_add_aad(&gcm,NULL,0);
  if((_crypt_libtom_err = gcm_process(&gcm,sig,256,open->body+256,GCM_ENCRYPT)) != CRYPT_OK) return packet_free(open);
  if((_crypt_libtom_err = gcm_done(&gcm, open->body+256+256, &len)) != CRYPT_OK) return packet_free(open);

  char khex[128], hex[2048];
  printf("KEY %s %s\n",util_hex(key,32,khex),util_hex(open->body+256,260,hex));
  
  return open;
}

packet_t crypt_deopenize_2a(crypt_t self, packet_t open)
{
  unsigned char enc[256], sig[256], ecc[65], hecc[129], *eopen, *esig, key[32], hash[32], iv[16], *hiv, *hline, line[16], *rawinner, sigkey[65+16];
  unsigned long len, len2;
  int res;
  symmetric_CTR ctr;
  rsa_key rsa;
  packet_t inner = NULL;
  crypt_libtom_t cs = (crypt_libtom_t)self->cs;

  // grab the sig
  esig = (unsigned char*)packet_get_str(open,"sig");
  len = sizeof(enc);
  if(!esig || (_crypt_libtom_err = base64_decode(esig, strlen((char*)esig), enc, &len)) != CRYPT_OK) return NULL;

  // decrypt the open
  eopen = (unsigned char*)packet_get_str(open,"open");
  len = sizeof(enc);
  if(!eopen || (_crypt_libtom_err = base64_decode(eopen, strlen((char*)eopen), enc, &len)) != CRYPT_OK) return NULL;
  len2 = sizeof(ecc);
  if((_crypt_libtom_err = rsa_decrypt_key(enc, len, ecc, &len2, 0, 0, find_hash("sha1"), &res, &(cs->rsa))) != CRYPT_OK || len2 != 65) return NULL;

  // create the aes key/iv to decipher the body
  len = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),ecc,65,key,&len)) != CRYPT_OK) return NULL;
  hiv = (unsigned char*)packet_get_str(open,"iv");
  if(!hiv || strlen((char*)hiv) != 32) return NULL;
  util_unhex(hiv,32,iv);

  // create aes cipher now and decrypt the inner
  if((_crypt_libtom_err = ctr_start(find_cipher("aes"),iv,key,32,0,CTR_COUNTER_BIG_ENDIAN,&ctr)) != CRYPT_OK) return NULL;
  rawinner = malloc(open->body_len);
  if((_crypt_libtom_err = ctr_decrypt(open->body,rawinner,open->body_len,&ctr)) == CRYPT_OK) inner = packet_parse(rawinner,open->body_len);
  free(rawinner);
  if(!inner) return NULL;

  // decipher/verify the signature
  if((_crypt_libtom_err = rsa_import(inner->body, inner->body_len, &rsa)) != CRYPT_OK) return NULL;
  hline = (unsigned char*)packet_get_str(inner,"line");
  util_unhex(hline,32,line);
  memcpy(sigkey,ecc,65);
  memcpy(sigkey+65,line,16);
  len2 = 32;
  if((_crypt_libtom_err = hash_memory(find_hash("sha256"),sigkey,65+16,key,&len2)) != CRYPT_OK
    || (_crypt_libtom_err = ctr_start(find_cipher("aes"),iv,key,32,0,CTR_COUNTER_BIG_ENDIAN,&ctr)) != CRYPT_OK
    || (_crypt_libtom_err = ctr_decrypt(enc,sig,len,&ctr)) != CRYPT_OK
    || (_crypt_libtom_err = hash_memory(find_hash("sha256"),open->body,open->body_len,hash,&len2)) != CRYPT_OK
    || (_crypt_libtom_err = rsa_verify_hash_ex(sig, len, hash, len2, LTC_PKCS_1_V1_5, find_hash("sha256"), 0, &res, &rsa)) != CRYPT_OK
    || res != 1) {
      packet_free(inner);
      return NULL;
  }

  // stash the hex line key w/ the inner
  util_hex(ecc+1,64,hecc);
  packet_set_str(inner,"ecc",(char*)hecc);

  return inner;
}

