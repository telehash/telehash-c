#include <stdint.h>
#include <stdbool.h>
#include <string.h>
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
  end = encoded+(len-1);
  
  // make sure the dot separators are there
  dot1 = strchr(encoded,'.');
  if(!dot1 || dot1+1 >= end) return LOG_INFO("missing/bad first separator");
  dot1++;
  dot2 = strchr(dot1,'.');
  if(!dot2 || (dot2+1) >= end) return LOG_INFO("missing/bad second separator");
  dot2++;

  // quick sanity check of the base64
  if(!base64_decoder(dot2, (end-dot2)+1, NULL)) return LOG_INFO("invalid sig base64: %.*s",(end-dot2)+1,dot2);
  if(!base64_decoder(dot1, (dot2-dot1)-1, NULL)) return LOG_INFO("invalid claims base64: %.*s",(dot2-dot1)-1,dot1);
  if(!base64_decoder(encoded, (dot1-encoded)-1, NULL)) return LOG_INFO("invalid header b64: %.*s",(dot1-encoded)-1,encoded);

  claims = lob_new();
  header = lob_link(NULL, claims);
  
  // decode claims json
  lob_head(claims, NULL, base64_decoder(dot1, (dot2-dot1)-1, NULL));
  base64_decoder(dot1, (dot2-dot1)-1, lob_head_get(claims));

  // decode claims sig 
  lob_body(claims, NULL, base64_decoder(dot2, (end-dot2)+1, NULL));
  base64_decoder(dot2, (end-dot2)+1, lob_body_get(claims));

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
  
  LOG("checking %lu %.*s",lob_body_len(token),dot-encoded,encoded);

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
lob_t jwk_get(e3x_self_t self, uint8_t id, bool private)
{
  if(!self || !id) return LOG("missing/bad args");
  e3x_cipher_t cs = e3x_cipher_set(id,NULL);
  if(!cs || !cs->local_sign || !cs->alg || !strstr(cs->alg,"JWK")) return LOG("no support for %x",id);
  local_t local = self->locals[cs->id];
  if(!local) return LOG("no keys for %x",id);

  // build args
  lob_t args = lob_new();
  lob_set(args,"alg","JWK");
  if(private) lob_set_int(args,"private",1);
  lob_t jwk = cs->local_sign(local,args,NULL,0);
  lob_free(args);
  return jwk;
}