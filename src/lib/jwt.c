#include "jwt.h"

// one JWT is two chained LOB packets
//  token->head is the JWT header JSON
//  token->body is a raw LOB for the claims
//  claims->head is the JWT claims JSON
//  claims->body is the JWT signature

// base64
lob_t jwt_decode(char *encoded)
{
  return NULL;
}

// from raw lobs
lob_t jwt_parse(uint8_t *raw, uint32_t len)
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
