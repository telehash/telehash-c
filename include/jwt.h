#ifndef jwt_h
#define jwt_h

#include <stdint.h>
#include "lob.h"
#include "e3x.h"

// utils to parse and generate JWTs to/from LOBs

// one JWT is two chained LOB packets
//  token->head is the JWT header JSON
//  token->body is a raw LOB for the claims
//  claims->head is the JWT claims JSON
//  claims->body is the JWT signature

lob_t jwt_decode(char *encoded, size_t len); // base64
lob_t jwt_parse(uint8_t *raw, size_t len); // from raw lobs
lob_t jwt_claims(lob_t token); // just returns the token->chain

char *jwt_encode(lob_t token); // char* is cached/freed inside token
uint8_t *jwt_raw(lob_t token); // lob-encoded raw bytes of whole thing
uint32_t jwt_len(lob_t token); // length of raw bytes

lob_t jwt_verify(lob_t token, e3x_exchange_t x);
lob_t jwt_sign(lob_t token, e3x_self_t self);

// return >0 if this alg is supported
uint8_t jwt_alg(char *alg);

#endif
