#include "crypt_libtom.h"

struct crypt_libtom_struct
{
  rsa_key rsa;
  ecc_key eccOut;
  unsigned char eccIn[64];
};

int crypt_init_2a()
{
  return crypt_libtom_init();
}

crypt_t crypt_new_2a(unsigned char *key, int len)
{
  int der_len = 512;
  unsigned long hnlen=32;
  unsigned char der[der_len];
  
  if(!key || !len || len > der_len) return NULL;

  crypt_t c = malloc(sizeof (struct crypt_struct));
  bzero(c, sizeof (struct crypt_struct));
  
  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, (unsigned long*)&der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  // try to load rsa public key
  if((_crypt_err = rsa_import(der, der_len, &(c->rsa))) != CRYPT_OK) {
    free(c);
    return NULL;
  }

  // re-export it to be safe
  der_len = crypt_der(c, der, 2048);
  if(!der_len) {
    free(c);
    return NULL;
  }
  
  // generate hashname
  if((_crypt_err = hash_memory(find_hash("sha256"),der,der_len,c->hashname,&hnlen)) != CRYPT_OK){
    free(c);
    return NULL;
  }

  return c;
}

int crypt_public_2a(crypt_t c, unsigned char *der, int len)
{
  if(!c || !der) return 0;
  if((_crypt_err = rsa_export(der, (unsigned long*)&len, PK_PUBLIC, &(c->rsa))) != CRYPT_OK) return 0;
  return len;
}

void crypt_free_2a(crypt_t c)
{
  free(c);
}

int crypt_private_2a(crypt_t c, unsigned char *key, int len)
{
  unsigned long der_len = 4096;
  unsigned char der[der_len];
  
  if(!c) return 1;
  if(c->private) return 0; // already loaded

  if(!key || !len || len > der_len) return 1;

  // try to base64 decode in case that's the incoming format
  if(base64_decode(key, len, der, &der_len) != CRYPT_OK) {
    memcpy(der,key,len);
    der_len = len;
  }
  
  if((_crypt_err = rsa_import(der, der_len, &(c->rsa))) != CRYPT_OK) return 1;
  c->private = 1;
  return 0;
}

packet_t crypt_lineize_2a(crypt_t self, crypt_t c, packet_t p)
{
  packet_t line;
  unsigned char iv[16], hex[33], *enc;
  symmetric_CTR ctr;

  if(!c || !p || !c->lined) return NULL;
  line = packet_chain(p);
  crypt_rand(iv,16);
  packet_set_str(line,"type","line");
  packet_set_str(line,"iv", (char*)util_hex(iv,16,hex));
  packet_set_str(line,"line", (char*)util_hex(c->lineIn,16,hex));

  // create aes cipher now and encrypt
  if((_crypt_err = ctr_start(find_cipher("aes"), iv, c->keyOut, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(line);
  enc = malloc(packet_len(p));
  if((_crypt_err = ctr_encrypt(packet_raw(p),enc,packet_len(p),&ctr)) != CRYPT_OK)
  {
    free(enc);
    return packet_free(line);
  }
  packet_body(line,enc,packet_len(p));
  free(enc);

  return line;
}

packet_t crypt_delineize_2a(crypt_t self, crypt_t c, packet_t p)
{
  packet_t line;
  unsigned char iv[16], *dec;
  char *hiv;
  symmetric_CTR ctr;

  if(!c || !self || !p) return NULL;
  if(!c->lined) return packet_free(p);

  hiv = packet_get_str(p, "iv");
  if(strlen(hiv) != 32) return packet_free(p);
  util_unhex((unsigned char*)hiv,32,iv);

  // create aes cipher now and encrypt
  if((_crypt_err = ctr_start(find_cipher("aes"), iv, c->keyIn, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(p);
  dec = malloc(p->body_len);
  if((_crypt_err = ctr_decrypt(p->body,dec,p->body_len,&ctr)) != CRYPT_OK)
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
int crypt_lineinit_2a(crypt_t c)
{
  unsigned char secret[32], input[64];
  unsigned long len;
  ecc_key eccIn;

  if(!c) return 0;

  // create transient crypto stuff
  if(!c->atOut)
  {
    if((_crypt_err = ecc_make_key(&_crypt_prng, find_prng("yarrow"), 32, &(c->eccOut))) != CRYPT_OK) return 0;
    crypt_rand(c->lineOut,16);
    util_hex(c->lineOut,16,c->lineHex);
    c->atOut = (unsigned long)time(0);
  }
  
  // can't make a line w/o their stuff yet
  if(!c->atIn) return 0;
  
  // do the diffie hellman
  if((_crypt_err = ecc_ansi_x963_import(c->eccIn, 65, &eccIn)) != CRYPT_OK) return 0;
  len = sizeof(secret);
  if((_crypt_err = ecc_shared_secret(&(c->eccOut),&eccIn,secret,&len)) != CRYPT_OK) return 0;

  // make line keys!
  memcpy(input,secret,32);
  memcpy(input+32,c->lineOut,16);
  memcpy(input+48,c->lineIn,16);
  len = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"), input, 64, c->keyOut, &len)) != CRYPT_OK) return 0;
  memcpy(input,secret,32);
  memcpy(input+32,c->lineIn,16);
  memcpy(input+48,c->lineOut,16);
  len = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"), input, 64, c->keyIn, &len)) != CRYPT_OK) return 0;

  c->lined = 1;
  return 1;
}

// create a new open packet
packet_t crypt_openize_2a(crypt_t self, crypt_t c)
{
  unsigned char key[32], iv[16], hex[33], hn[65], sig[256], esig[256], pub[65], epub[256], b64[512], *enc;
  packet_t open, inner;
  unsigned long len, len2;
  symmetric_CTR ctr;

  if(!c || !self) return NULL;
  hex[32] = hn[64] = 0;

  // ensure line is setup
  crypt_lineinit(c);

  // create the inner/open packet
  inner = packet_new();
  packet_set_str(inner,"line",(char*)c->lineHex);
  packet_set_str(inner,"to",(char*)util_hex(c->hashname,32,hn));
  packet_set_int(inner,"at",(int)c->atOut);
  len = crypt_der(self, b64, 512);
  packet_body(inner,b64,len);

  open = packet_chain(inner);
  packet_set_str(open,"type","open");

  // create the aes iv and key from ecc
  crypt_rand(iv, 16);
  packet_set_str(open,"iv", (char*)util_hex(iv,16,hex));
  len = sizeof(pub);
  if((_crypt_err = ecc_ansi_x963_export(&(c->eccOut), pub, &len)) != CRYPT_OK) return packet_free(open);
  len = sizeof(key);
  if((_crypt_err = hash_memory(find_hash("sha256"), pub, 65, key, &len)) != CRYPT_OK) return packet_free(open);

  // create aes cipher now and encrypt the inner
  if((_crypt_err = ctr_start(find_cipher("aes"), iv, key, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(open);
  enc = malloc(packet_len(inner));
  if((_crypt_err = ctr_encrypt(packet_raw(inner),enc,packet_len(inner),&ctr)) != CRYPT_OK)
  {
    free(enc);
    return packet_free(open);
  }
  packet_body(open,enc,packet_len(inner));
  free(enc);

  // encrypt the ecc key and attach it
  len = sizeof(epub);
  if((_crypt_err = rsa_encrypt_key(pub, 65, epub, &len, 0, 0, &_crypt_prng, find_prng("yarrow"), find_hash("sha1"), &(c->rsa))) != CRYPT_OK) return packet_free(open);
  len2 = sizeof(b64);
  if((_crypt_err = base64_encode(epub, len, b64, &len2)) != CRYPT_OK) return packet_free(open);
  b64[len2] = 0;
  packet_set_str(open, "open", (char*)b64);

  // sign the inner packet
  len = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"),open->body,open->body_len,key,&len)) != CRYPT_OK) return packet_free(open);
  len = sizeof(sig);
  if((_crypt_err = rsa_sign_hash_ex(key, 32, sig, &len, LTC_PKCS_1_V1_5, &_crypt_prng, find_prng("yarrow"), find_hash("sha256"), 12, &(self->rsa))) != CRYPT_OK) return packet_free(open);

	// encrypt the signature, create the new aes key+cipher first
  memcpy(b64,pub,65);
  memcpy(b64+65,c->lineOut,16);
  len2 = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"), b64, 65+16, key, &len2)) != CRYPT_OK) return packet_free(open);
  if((_crypt_err = ctr_start(find_cipher("aes"), iv, key, 32, 0, CTR_COUNTER_BIG_ENDIAN, &ctr)) != CRYPT_OK) return packet_free(open);
  if((_crypt_err = ctr_encrypt(sig, esig, len, &ctr)) != CRYPT_OK) return packet_free(open);
  len2 = sizeof(b64);
  if((_crypt_err = base64_encode(esig, len, b64, &len2)) != CRYPT_OK) return packet_free(open);
  b64[len2] = 0;
  packet_set_str(open, "sig", (char*)b64);
  
  return open;
}

crypt_t crypt_deopenize_2a(crypt_t self, packet_t open)
{
  unsigned char enc[256], sig[256], pub[65], *eopen, *esig, key[32], hash[32], iv[16], *hiv, *hline, *rawinner, sigkey[65+16];
  unsigned long len, len2;
  int res;
  crypt_t c;
  symmetric_CTR ctr;
  packet_t inner;

  if(!open) return NULL;
  if(!self) return (crypt_t)packet_free(open);

  // decrypt the open
  eopen = (unsigned char*)packet_get_str(open,"open");
  len = sizeof(enc);
  if(!eopen || (_crypt_err = base64_decode(eopen, strlen((char*)eopen), enc, &len)) != CRYPT_OK) return (crypt_t)packet_free(open);;
  len2 = sizeof(pub);
  if((_crypt_err = rsa_decrypt_key(enc, len, pub, &len2, 0, 0, find_hash("sha1"), &res, &(self->rsa))) != CRYPT_OK || len2 != 65) return (crypt_t)packet_free(open);

  // create the aes key/iv to decipher the body
  len = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"),pub,65,key,&len)) != CRYPT_OK) return (crypt_t)packet_free(open);
  hiv = (unsigned char*)packet_get_str(open,"iv");
  if(!hiv || strlen((char*)hiv) != 32) return (crypt_t)packet_free(open);
  util_unhex(hiv,32,iv);

  // create aes cipher now and decrypt the inner
  if((_crypt_err = ctr_start(find_cipher("aes"),iv,key,32,0,CTR_COUNTER_BIG_ENDIAN,&ctr)) != CRYPT_OK) return (crypt_t)packet_free(open);
  rawinner = malloc(open->body_len);
  if((_crypt_err = ctr_decrypt(open->body,rawinner,open->body_len,&ctr)) != CRYPT_OK || (inner = packet_parse(rawinner,open->body_len)) == NULL)
  {
    free(rawinner);
    packet_free(open);
    return NULL;
  }
  free(rawinner);

  // extract/validate inner stuff
  c = crypt_new(inner->body,inner->body_len);
  if(!c) return (crypt_t)packet_free(open);
  c->atIn = strtol(packet_get_str(inner,"at"), NULL, 10);
  hline = (unsigned char*)packet_get_str(inner,"line");
  if(c->atIn <= 0 || strlen((char*)hline) != 32 || !c)
  {
    crypt_free(c);
    packet_free(inner);
    packet_free(open);
    return NULL;
  }
  util_unhex(hline,32,c->lineIn);
  packet_free(inner);
  memcpy(c->eccIn,pub,65);

  // decipher/verify the signature
  esig = (unsigned char*)packet_get_str(open,"sig");
  len = sizeof(enc);
  if(!esig || (_crypt_err = base64_decode(esig, strlen((char*)esig), enc, &len)) != CRYPT_OK)
  {
    crypt_free(c);
    packet_free(open);
    return NULL;
  }
  memcpy(sigkey,pub,65);
  memcpy(sigkey+65,c->lineIn,16);
  len2 = 32;
  if((_crypt_err = hash_memory(find_hash("sha256"),sigkey,65+16,key,&len2)) != CRYPT_OK
    || (_crypt_err = ctr_start(find_cipher("aes"),iv,key,32,0,CTR_COUNTER_BIG_ENDIAN,&ctr)) != CRYPT_OK
    || (_crypt_err = ctr_decrypt(enc,sig,len,&ctr)) != CRYPT_OK
    || (_crypt_err = hash_memory(find_hash("sha256"),open->body,open->body_len,hash,&len2)) != CRYPT_OK
    || (_crypt_err = rsa_verify_hash_ex(sig, len, hash, len2, LTC_PKCS_1_V1_5, find_hash("sha256"), 0, &res, &(c->rsa))) != CRYPT_OK
    || res != 1) {
      crypt_free(c);
      packet_free(open);
      return NULL;
  }
	packet_free(open);

  return c;
}

crypt_t crypt_merge_2a(crypt_t a, crypt_t b)
{
  memcpy(a->eccIn, b->eccIn, 64);
}
