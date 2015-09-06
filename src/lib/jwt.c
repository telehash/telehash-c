#include <string.h>
#include "telehash.h"
#include "telehash.h"
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
  size_t dlen;

  if(!encoded) return NULL;
  if(!len) len = strlen(encoded);
  end = encoded+(len-1);
  
  // make sure the dot separators are there
  dot1 = strchr(encoded,'.');
  if(!dot1 || dot1+1 >= end) return LOG("missing/bad first separator");
  dot1++;
  dot2 = strchr(dot1,'.');
  if(!dot2 || (dot2+1) >= end) return LOG("missing/bad second separator");
  dot2++;

  claims = lob_new();
  header = lob_link(NULL, claims);
  
  // decode claims sig first
  dlen = base64_decoder(dot2, (end-dot2)+1, (uint8_t*)dot2);
  lob_body(claims, (uint8_t*)dot2, dlen);

  // decode claims json
  dlen = base64_decoder(dot1, (dot2-dot1), (uint8_t*)dot1);
  lob_head(claims, (uint8_t*)dot1, dlen);

  // decode header json
  dlen = base64_decoder(encoded, (dot1-encoded), (uint8_t*)encoded);
  lob_head(header, (uint8_t*)encoded, dlen);

  return header;
}

// util to parse token from binary lob-encoding
lob_t jwt_parse(uint8_t *raw, size_t len)
{
  return NULL;
}

// just returns the token->chain payload claim
lob_t jwt_claims(lob_t token)
{
  return lob_linked(token);
}

// returns the base64 encoded token from a packet (return is cached/freed inside token)
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
  encoded = (char*)lob_body(token,NULL,hlen+1+clen+1+slen+1);
  
  // append all the base64 encoding
  hlen = base64_encoder(token->head,token->head_len,encoded);
  encoded[hlen] = '.';
  clen = base64_encoder(payload->head,payload->head_len,encoded+hlen+1);
  encoded[hlen+1+clen] = '.';
  slen = base64_encoder(payload->body,payload->body_len,encoded+hlen+1+clen+1);
  encoded[hlen+1+clen+1+slen] = 0;

  return encoded;
}

// lob-encoded raw bytes of a token
uint8_t *jwt_raw(lob_t token)
{
  return NULL;
}

// length of raw bytes
uint32_t jwt_len(lob_t token)
{
  return 0;
}

// verify the signature on this token, optionally using key loaded in this exchange
lob_t jwt_verify(lob_t token, e3x_exchange_t x)
{
  size_t hlen, clen;
  char *encoded;
  uint8_t err;
  lob_t payload = lob_linked(token);
  if(!token || !payload) return LOG("bad args");

  // generate the temporary encoded data
  clen = base64_encode_length(payload->head_len);
  hlen = base64_encode_length(token->head_len);
  encoded = (char*)malloc(hlen+1+clen);
  hlen = base64_encoder(token->head,token->head_len,encoded);
  encoded[hlen] = '.';
  clen = base64_encoder(payload->head,payload->head_len,encoded+hlen+1);

  // do the validation
  err = e3x_exchange_validate(x, token, payload, (uint8_t*)encoded, hlen+1+clen);
  free(encoded);

  if(err)
  {
    LOG("validate failed: %d",err);
    return NULL;
  }

  return token;
}

// sign this token, adds signature to the claims body
lob_t jwt_sign(lob_t token, e3x_self_t self)
{
  size_t hlen, clen;
  char *encoded;
  lob_t payload = lob_linked(token);
  if(!token || !payload) return LOG("bad args");

  // allocates space in the payload body for the encoded data to be signed
  clen = base64_encode_length(payload->head_len);
  hlen = base64_encode_length(token->head_len);
  encoded = (char*)lob_body(payload,NULL,hlen+1+clen);
  
  hlen = base64_encoder(token->head,token->head_len,encoded);
  encoded[hlen] = '.';
  clen = base64_encoder(payload->head,payload->head_len,encoded+hlen+1);

  // e3x returns packet w/ signature
  if(!e3x_self_sign(self, token, payload->body, hlen+1+clen)) return LOG("signing failed");
  
  // copy sig to payload
  lob_body(payload,token->body,token->body_len);
  return token;
}

// return >0 if this alg is supported
uint8_t jwt_alg(char *alg)
{
  uint8_t err;
  lob_t test = lob_new();
  lob_set(test,"alg",alg);
  // TODO refactor how this is checked
  err = e3x_exchange_validate(NULL, test, NULL, (uint8_t*)"x", 1);
  lob_free(test);
  return (err == 1) ? 0 : 1;
}
