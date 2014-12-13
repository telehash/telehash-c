#ifndef util_uri_h
#define util_uri_h

#include <stdint.h>
#include "lob.h"

// utils to parse and generate JWTs to/from LOBs

// one JWT is two chained LOB packets
//  token->head is the JWT header JSON
//  token->body is a raw LOB for the claims
//  claims->head is the JWT claims JSON
//  claims->body is the JWT signature

lob_t jwt_decode(char *encoded); // base64
lob_t jwt_parse(uint8_t *raw, uint32_t len); // from raw lobs
lob_t jwt_claims(lob_t token); // just returns the token->chain

char *jwt_encode(lob_t token); // char* is cached/freed inside token
uint8_t *jwt_raw(lob_t token); // lob-encoded raw bytes of whole thing
uint32_t jwt_len(lob_t token); // length of raw bytes

// TODO, WIP as to _how_ to do this yet
lob_t jwt_verify(lob_t token, lob_t key); // probably use e3x_exchange_verify and have it recognize?
lob_t jwt_sign(lob_t token, lob_t key); // probably add e3x_self_sign?

#endif
