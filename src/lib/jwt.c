#include <string.h>
#include "util.h"
#include "base64.h"
#include "jwt.h"

// one JWT is two chained LOB packets
//  token->head is the JWT header JSON
//  token->body is a raw LOB for the claims
//  claims->head is the JWT claims JSON
//  claims->body is the JWT signature

// base64
lob_t jwt_decode(char *encoded, size_t len)
{
  lob_t header, payload;
  char *dot1, *dot2, *end;
  size_t dlen;

  if(!encoded) return NULL;
  if(!len) len = strlen(encoded);
  end = encoded+(len-1);
  
  dot1 = strchr(encoded,'.');
  if(!dot1 || dot1+1 >= end) return LOG("missing/bad first separator");
  dot1++;
  dot2 = strchr(dot1,'.');
  if(!dot2 || (dot2+1) >= end) return LOG("missing/bad second separator");
  dot2++;

  payload = lob_new();
  header = lob_link(NULL, payload);
  
  // decode payload body first
  dlen = base64_decode(dot2, (end-dot2)+1, (uint8_t*)dot2);
  lob_body(payload, (uint8_t*)dot2, dlen);

  // decode payload json
  dlen = base64_decode(dot1, (dot2-dot1), (uint8_t*)dot1);
  lob_head(payload, (uint8_t*)dot1, dlen);

  // decode header json
  dlen = base64_decode(encoded, (dot1-encoded), (uint8_t*)encoded);
  lob_head(header, (uint8_t*)encoded, dlen);

  return header;
}

// from raw lobs
lob_t jwt_parse(uint8_t *raw, size_t len)
{
  return NULL;
}

// just returns the token->chain
lob_t jwt_claims(lob_t token)
{
  return NULL;
}

// char* is cached/freed inside token
char *jwt_encode(lob_t token)
{
  return NULL;
}

// lob-encoded raw bytes of whole thing
uint8_t *jwt_raw(lob_t token)
{
  return NULL;
}

// length of raw bytes
uint32_t jwt_len(lob_t token)
{
  return 0;
}

// TODO, WIP as to _how_ to do this yet
// probably use e3x_exchange_verify and have it recognize?
lob_t jwt_verify(lob_t token, lob_t key)
{
  return NULL;
}

// probably add e3x_self_sign?
lob_t jwt_sign(lob_t token, lob_t key)
{
  return NULL;
}
