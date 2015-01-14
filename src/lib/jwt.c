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
  lob_t header, claims;
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

  claims = lob_new();
  header = lob_link(NULL, claims);
  
  // decode claims sig first
  dlen = base64_decode(dot2, (end-dot2)+1, (uint8_t*)dot2);
  lob_body(claims, (uint8_t*)dot2, dlen);

  // decode claims json
  dlen = base64_decode(dot1, (dot2-dot1), (uint8_t*)dot1);
  lob_head(claims, (uint8_t*)dot1, dlen);

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
  return lob_linked(token);
}

// char* is cached/freed inside token
char *jwt_encode(lob_t token)
{
  size_t hlen, clen, slen;
  char *encoded;
  lob_t payload = lob_linked(token);
  if(!payload) return NULL;
  
  // allocates space in the token body
  slen = base64_encode_length(payload->body_len);
  clen = base64_encode_length(payload->head_len);
  hlen = base64_encode_length(token->head_len);
  encoded = (char*)lob_body(token,NULL,3+hlen+clen+slen);
  
  hlen = base64_encode(token->head,token->head_len,encoded);
  encoded[hlen] = '.';
  clen = base64_encode(payload->head,payload->head_len,encoded+hlen+1);
  encoded[hlen+1+clen] = '.';
  slen = base64_encode(payload->body,payload->body_len,encoded+hlen+1+clen+1);
  encoded[hlen+1+clen+1+slen] = 0;

  return encoded;
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


lob_t jwt_verify(lob_t token, e3x_exchange_t x)
{
  // use e3x_exchange_validate
  return NULL;
}

lob_t jwt_sign(lob_t token, e3x_self_t self)
{
  // use e3x_self_sign
  return NULL;
}
