#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "telehash.h"

// this lib implements basic JWT support using the crypto/lob utilities in telehash-c

// one JWT is two chained packets using lob.c
//  token->head is the JWT header JSON
//  token->body is a raw LOB for the claims
//  claims->head is the JWT claims JSON
//  claims->body is the JWT signature

// decode base64 into the pair of lob packets
lob_t jwt_decode(char *encoded, size_t len)
{
  lob_t header, claims;
  char *dot1, *dot2, *end;

  if(!encoded) return NULL;
  if(!len) len = strlen(encoded);
  end = encoded+len;
  
  // make sure the dot separators are there
  dot1 = memchr(encoded,'.',(end-encoded));
  if(!dot1) return LOG_INFO("missing/bad first separator");
  dot1++;
  dot2 = memchr(dot1,'.',(end-dot1));
  if(!dot2) return LOG_INFO("missing/bad second separator");
  dot2++;

  // quick sanity check of the base64
  if(!base64_decoder(dot2, (end-dot2), NULL)) return LOG_INFO("invalid sig base64: %.*s",(end-dot2),dot2);
  if(!base64_decoder(dot1, (dot2-dot1)-1, NULL)) return LOG_INFO("invalid claims base64: %.*s",(dot2-dot1)-1,dot1);
  if(!base64_decoder(encoded, (dot1-encoded)-1, NULL)) return LOG_INFO("invalid header b64: %.*s",(dot1-encoded)-1,encoded);

  claims = lob_new();
  header = lob_link(NULL, claims);
  
  // decode claims json
  lob_head(claims, NULL, base64_decoder(dot1, (dot2-dot1)-1, NULL));
  base64_decoder(dot1, (dot2-dot1)-1, lob_head_get(claims));

  // decode claims sig 
  lob_body(claims, NULL, base64_decoder(dot2, (end-dot2), NULL));
  base64_decoder(dot2, (end-dot2), lob_body_get(claims));

  // decode header json
  lob_head(header, NULL, base64_decoder(encoded, (dot1-encoded)-1, NULL));
  base64_decoder(encoded, (dot1-encoded)-1, lob_head_get(header));

  return header;
}

// util to parse token from binary lob-encoding
lob_t jwt_parse(uint8_t *raw, size_t len)
{
  return NULL;
}

// just returns the token->chain claims claim
lob_t jwt_claims(lob_t token)
{
  return lob_linked(token);
}

// returns the base64 encoded token from a packet
char *jwt_encode(lob_t token)
{
  lob_t claims = jwt_claims(token);
  if(!claims) return LOG_WARN("no claims");
  
  size_t slen = base64_encode_length(claims->body_len);
  size_t clen = base64_encode_length(claims->head_len);
  size_t hlen = base64_encode_length(token->head_len);
  char *encoded;
  if(!(encoded = malloc(hlen+1+clen+1+slen+1))) return LOG_WARN("OOM");
  
  // append all the base64 encoding
  hlen = base64_encoder(token->head,token->head_len,encoded);
  encoded[hlen] = '.';
  clen = base64_encoder(claims->head,claims->head_len,encoded+hlen+1);
  encoded[hlen+1+clen] = '.';
  slen = base64_encoder(claims->body,claims->body_len,encoded+hlen+1+clen+1);
  encoded[hlen+1+clen+1+slen] = 0;

  return encoded;
}

// lob-encoded raw bytes of a token
uint8_t *jwt_raw(lob_t token)
{
  return LOG_INFO("TODO");
}

// length of raw bytes
uint32_t jwt_len(lob_t token)
{
  return 0;
}

// verify the signature on this token, optionally using key loaded in this exchange
lob_t jwt_verify(lob_t token, e3x_exchange_t x)
{
  lob_t claims = jwt_claims(token);
  if(!token || !claims) return LOG("bad args");

  // generate the temporary encoded data
  char *encoded = jwt_encode(token);
  if(!encoded) return LOG("bad token");
  char *dot = strchr(encoded,'.');
  dot = strchr(dot+1,'.');
  
  LOG("checking %lu %.*s",lob_body_len(claims),dot-encoded,encoded);

  // do the validation against the sig on the claims using info from the token
  uint8_t err = e3x_exchange_validate(x, token, claims, (uint8_t*)encoded, dot-encoded);
  free(encoded);

  if(err) return LOG("validate failed: %d",err);
  return token;
}

// sign this token, adds signature to the claims body
lob_t jwt_sign(lob_t token, e3x_self_t self)
{
  lob_t claims = jwt_claims(token);
  if(!token || !claims) return LOG("bad args");

  // generate the temporary encoded data
  char *encoded = jwt_encode(token);
  if(!encoded) return LOG("bad token");
  char *dot = strchr(encoded,'.');
  dot = strchr(dot+1,'.');

  // e3x returns token w/ signature as the body
  if(!e3x_self_sign(self, token, (uint8_t*)encoded, dot-encoded)) return LOG("signing failed");
  free(encoded);
  
  // move sig to claims
  lob_body(claims,token->body,token->body_len);
  lob_body(token,NULL,0);

  return token;
}

// if this alg is supported
char *jwt_alg(char *alg)
{
  if(!alg) return LOG("missing arg");
  return (e3x_cipher_set(0,alg)) ? alg : NULL;
}

// we're overloading local_sign() right now until a refactor
lob_t jwk_local_get(e3x_self_t self, lob_t jwk, bool private)
{
  if(!self) return LOG("missing/bad args");
  for(uint8_t i=0; i<CS_MAX; i++)
  {
    e3x_cipher_t cs = e3x_cipher_sets[i];
    if(!cs || !self->locals[i] || !cs->local_sign || !cs->alg || !strstr(cs->alg,"JWK")) continue;
    lob_t ret = cs->local_sign(self->locals[i],jwk,NULL,private?1:0); // OVERLOADED (refactor me)
    if(ret) return ret;
  }
  
  return NULL;
}

e3x_self_t jwk_local_load(lob_t jwk, bool generate)
{
  if(!jwk || !lob_get(jwk,"kty")) return LOG("missing/bad args");
  lob_t gen = NULL;
  if(generate) gen = lob_new(); // OVERLOADED for gen flag
  e3x_self_t self = e3x_self_new(gen, jwk);
  lob_free(gen);
  return self;
}

lob_t jwk_remote_get(e3x_exchange_t x, lob_t jwk)
{
  return NULL;
}

e3x_exchange_t jwk_remote_load(lob_t jwk)
{
  if(!jwk || !lob_get(jwk,"kty")) return LOG("missing/bad args");

  // this is such a (TEMPORARY) hack, :(
  for(uint8_t i=0; i<CS_MAX; i++)
  {
    e3x_cipher_t cs = e3x_cipher_sets[i];
    remote_t remote = NULL;
    if(!cs || !cs->remote_new || !cs->alg || !strstr(cs->alg,"JWK")) continue;
    if(!(remote = cs->remote_new(jwk,NULL))) continue;
    free(remote);
    return e3x_exchange_new(NULL, cs->csid, jwk);
  }

  return LOG_WARN("unsupported JWK: %s",lob_json(jwk));
}


/*
{
  "header": {
    "alg": "ECDH-EH+A128CTR-HS256",
    "enc": "A128CTR-HS256",
    "hks": "akrFmMyGcsF3nQWxpY2U",
    "tag": "Mz-VPPyU4RlcuYv1IwIvzw",
    "iv": "AxY8DCtDaGlsbGljb3RoZQ",
    "epk": {
      "kty": "EC",
      "crv": "P-256",
      "x": "gI0GAILBdu7T53akrFmMyGcsF3n5dO7MmwNBHKW5SV0",
      "y": "SLW_xSffzlPWrHEVI30DHM_4egVwt3NQqeUD7nMFpps"
    }
  },
  "encrypted_key": "6KB707dM9YTIgHtLvtgWQ8mKwboJW3of9locizkDTHzBC2IlrT1oOQ",
  "iv": "AxY8DCtDaGlsbGljb3RoZQ",
  "ciphertext": "KDlTtXchhZTGufMYmOYGS4HffxPSUrfmqCHXaI9wOGY",
  "tag": "Mz-VPPyU4RlcuYv1IwIvzw"
}
*/

lob_t jwe_encrypt_1c(e3x_exchange_t to, lob_t jwt, uint8_t *ckey)
{
  uint8_t buf[16];

  // generate ephemeral EC
  lob_t ek = lob_new();
  lob_set(ek,"kty","EC");  lob_set(ek,"crv","P-256");
  e3x_self_t ephem = jwk_local_load(ek, true);
  ek = lob_free(ek);

  // get just the public JWK of it
  lob_t epk = lob_new();
  lob_set(epk,"kty","EC");
  lob_set(epk,"crv","P-256");
  jwk_local_get(ephem, epk, false);

  // create the header
  lob_t header = lob_new();
  lob_set(header,"alg","ECDH-EH+A128CTR-HS256");
  lob_set(header,"enc","A128CTR-HS256");
  e3x_rand(buf,8);
  lob_set_base64(header,"hks",buf,8);
  e3x_rand(buf,16);
  lob_set_base64(header,"iv",buf,16);
  lob_set_raw(header,"epk",3,lob_json(epk),0);
  epk = lob_free(epk);
  
  // encrypt the shared key first
  lob_body(header,ckey,32);
  
  // let the JWE crypto backend generate a tag and ciphertext key
  lob_t key = lob_new();
  lob_set(key,"req","ECDH HK256 A128CTR HS256");
  lob_link(header,key);
  if(!to->cs->remote_encrypt(to->remote,ephem->locals[to->cs->id],header))
  {
    LOG_WARN("JWE failed for %s",lob_json(header));
    return lob_free(header);
  }
  lob_unlink(header); // detach key
  
  lob_t jwe = lob_new();
  lob_set_base64(jwe,"encrypted_key",lob_body_get(header),lob_body_len(header));
  lob_set_raw(jwe,"header",6,lob_json(header),0);

  char *encoded = jwt_encode(jwt);
  lob_body(jwe,(uint8_t*)encoded,strlen(encoded));
  e3x_rand(buf,16);
  lob_set_base64(jwe,"iv",buf,16);
  
  // encrypt outer part of the JWE
  lob_set(key,"req","A128CTR HS256");
  lob_body(key,ckey,32);
  lob_link(jwe,key);
  if(!to->cs->remote_encrypt(to->remote,ephem,jwe))
  {
    LOG_WARN("JWE failed for %s",lob_json(header));
    return lob_free(header);
  }
  lob_unlink(jwe); // detach key
  lob_free(key);
  lob_link(jwe,header); // return orig header linked
  
  lob_set_base64(jwe,"ciphertext",lob_body_get(jwe),lob_body_len(jwe));
  lob_body(jwe,NULL,0);
  
  return jwe;
}

lob_t jwe_decrypt_1c(e3x_self_t self, lob_t jwe, uint8_t *ckey)
{
  e3x_cipher_t cs = e3x_cipher_set(0x1c,NULL);
  if(!cs || !self || !self->locals[cs->id]) return LOG("cs1c unavailable");

  lob_t header = lob_get_json(jwe,"header");
  
  // decrypt the shared key first using the header
  lob_t ekey = lob_get_base64(jwe,"encrypted_key");
  lob_body(header,lob_body_get(ekey),lob_body_len(ekey));
  
  lob_t key = lob_new();
  lob_set(key,"req","ECDH HK256 A128CTR HS256");
  lob_link(header,key);
  if(!cs->local_decrypt(self->locals[cs->id],header))
  {
    LOG_WARN("JWE failed for %s",lob_json(header));
    return lob_free(header);
  }
  lob_unlink(header); // detach key
  memcpy(ckey,lob_body_get(header),32);
  
  lob_t ctext = lob_get_base64(jwe,"ciphertext");
  lob_body(jwe,lob_body_get(ctext),lob_body_len(ctext));
  
  lob_set(key,"req","A128CTR HS256");
  lob_body(key,ckey,32);
  lob_link(jwe,key);
  if(!cs->local_decrypt(self->locals[cs->id],jwe))
  {
    LOG_WARN("JWE failed for %s",lob_json(jwe));
    return NULL;
  }
  lob_unlink(jwe); // detach key

  return jwt_decode((char*)lob_body_get(jwe),lob_body_len(jwe));
}
