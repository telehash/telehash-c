#include "jwt.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  char jwt[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOjEyMzQ1Njc4OTAsIm5hbWUiOiJKb2huIERvZSIsImFkbWluIjp0cnVlfQ.eoaDVGTClRdfxUZXiPs3f8FmJDkDE_VCQFXqKxpLsts";

  lob_t head = jwt_decode(jwt, 0);
  fail_unless(head);
  LOG("head %s",lob_json(head));
  fail_unless(lob_get(head,"typ"));
  fail_unless(lob_get(head,"alg"));
  lob_t payload = lob_linked(head);
  fail_unless(payload);
  fail_unless(lob_get(payload,"sub"));
  fail_unless(lob_get(payload,"name"));
  fail_unless(payload->body_len == 32);
  
  char jwt2[] = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOjQyfQ.GMQg9LfZNenJCrdWEDklQRxHqCHgYC5DN9YkdetGurKAxITJ1552lKwkgRPUJMNE9kHYW2yslC42sjhZ50m0n25zuOxt5rzbonotFb243deLt5bjvLP1HxaA37qtjGVvPXPwI-nquSVKrVAndaB8p16Beo_Ftehneu_ClFbkOpUYmalVGzIhI3MjnwYusFkEPK7LWOTIe8S_UZFaob6uSiGo_6q3Fwn84QkU2pOGlUAg3onJGKLOlXdGhZDrpjvYJtJXPpRj_1cxTTBAWlAX28-jUEi0U1EpN_vswLlGAx-ZgARAiMww2_z6GNcj2k7Tnnju2BOYL5RFGCqIob7jhw";
  head = jwt_decode(jwt2, 0);
  fail_unless(head);
  fail_unless(lob_get(head,"typ"));
  fail_unless(lob_get(head,"alg"));
  payload = lob_linked(head);
  fail_unless(payload);
  fail_unless(lob_get(payload,"sub"));
  fail_unless(payload->body_len == 256);

  char *enc = jwt_encode(head);
  fail_unless(enc);
  fail_unless(util_cmp(jwt2,enc) == 0);
  
  // test signing
  fail_unless(e3x_init(NULL) == 0);

  if(jwt_alg("HS256"))
  {
    lob_t hs256 = lob_new();
    lob_set(hs256,"alg","HS256");
    lob_set(hs256,"typ","JWT");
    lob_t hsp = lob_new();
    lob_set_int(hsp,"sub",42);
    lob_link(hs256,hsp);
    // set hmac secret on token body
    lob_body(hs256,(uint8_t*)"secret",6);
    fail_unless(jwt_sign(hs256,NULL));
    fail_unless(hsp->body_len == 32);
    LOG("signed JWT: %s",jwt_encode(hs256));
    lob_body(hs256,(uint8_t*)"secret",6);
    fail_unless(jwt_verify(hs256,NULL));
  }

  // test real signing using generated keys
  lob_t id = e3x_generate();
  e3x_self_t self = e3x_self_new(id,NULL);
  fail_unless(self);

  if(jwt_alg("ES160"))
  {
    lob_t es160 = lob_new();
    lob_set(es160,"alg","ES160");
    lob_set(es160,"typ","JWT");
    lob_t esp = lob_new();
    lob_set_int(esp,"sub",42);
    lob_link(es160,esp);
    fail_unless(jwt_sign(es160,self));
    fail_unless(esp->body_len == 40);
    LOG("signed JWT: %s",jwt_encode(es160));

    lob_t key = lob_get_base32(lob_linked(id),"1a");
    e3x_exchange_t x = e3x_exchange_new(self, 0x1a, key);
    fail_unless(x);
    fail_unless(jwt_verify(es160,x));
  }

  if(jwt_alg("RS256"))
  {
    lob_t test = lob_new();
    lob_set(test,"alg","RS256");
    lob_set(test,"typ","JWT");
    lob_t testp = lob_new();
    lob_set_int(testp,"sub",42);
    lob_link(test,testp);
    fail_unless(jwt_sign(test,self));
    fail_unless(testp->body_len == 256);
    char *encoded = jwt_encode(test);
    fail_unless(encoded);
    LOG("signed JWT: %s",encoded);

    lob_t key = lob_get_base32(lob_linked(id),"2a");
    e3x_exchange_t x = e3x_exchange_new(self, 0x2a, key);
    fail_unless(x);
    fail_unless(jwt_verify(test,x));
  }

  if(jwt_alg("ES256"))
  {
    lob_t es256 = lob_new();
    lob_set(es256,"alg","ES256");
    lob_set(es256,"typ","JWT");
    lob_t esp = lob_new();
    lob_set_int(esp,"sub",42);
    lob_link(es256,esp);
    fail_unless(jwt_sign(es256,self));
    fail_unless(esp->body_len == 64);
    LOG("signed JWT: %s",jwt_encode(es256));

    lob_t key = lob_get_base32(lob_linked(id),"1c");
    e3x_exchange_t x = e3x_exchange_new(self, 0x1c, key);
    fail_unless(x);
    fail_unless(jwt_verify(es256,x));
    
    lob_t jwk = lob_new();
    lob_set(jwk,"kty","EC");
    lob_set(jwk,"crv","P-256");
    fail_unless(jwk_local_get(self,jwk,false));
    LOG_DEBUG("JWK: %s",lob_json(jwk));
    fail_unless(lob_get(jwk,"x"));
    fail_unless(lob_get(jwk,"y"));
    
    fail_unless(jwk_local_get(self,jwk,true));
    e3x_self_t kself = jwk_local_load(jwk,false);
    fail_unless(kself);

    jwk = lob_new();
    lob_set(jwk,"kty","EC");
    lob_set(jwk,"crv","P-256");
    kself = jwk_local_load(jwk,true);
    fail_unless(kself);
    fail_unless(lob_get(jwk,"x"));
    fail_unless(lob_get(jwk,"d"));
    
    e3x_exchange_t kx = jwk_remote_load(jwk);
    fail_unless(kx);

    uint8_t ckey[32] = "just testing";
    LOG_DEBUG("key: %s",util_hex(ckey,32,NULL));
    lob_t jwe = jwe_encrypt_1c(kx, es256, ckey);
    fail_unless(jwe);
    printf("JWE: %s\n",lob_json(jwe));
    fail_unless(lob_get(jwe,"header"));
    fail_unless(lob_get(jwe,"ciphertext"));
    
    memset(ckey,0,32);
    lob_t jwt = jwe_decrypt_1c(kself,jwe,ckey);
    fail_unless(jwt);
    LOG_DEBUG("deciphered JWT: %s",lob_json(jwt));
    fail_unless(memcmp(ckey,"just testing",12) == 0);
    fail_unless(jwt_verify(jwt,x));
  }

  // brunty
  int i = 1000;
  char *tok = strdup(jwt);
  while(i--)
  {
    lob_t t = jwt_decode(tok,0);
    free(tok);
    tok = jwt_encode(t);
    lob_free(t);
    fail_unless(tok);
  }

  return 0;
}

