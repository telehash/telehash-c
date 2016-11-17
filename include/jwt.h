#ifndef jwt_h
#define jwt_h

#include <stdint.h>
#include <stdbool.h>
#include "lob.h"
#include "e3x.h"

// utils to parse and generate JWTs to/from LOBs

// one JWT is two LOB packets, jwt_claims() returns second one
//  token->head is the JWT header JSON
//  token->body is a raw LOB for the claims
//  claims->head is the JWT claims JSON
//  claims->body is the JWT signature

lob_t jwt_decode(char *encoded, size_t len); // base64 into lobs
char *jwt_encode(lob_t token); // returns new malloc'd base64 string, caller owns

lob_t jwt_parse(uint8_t *raw, size_t len); // from raw lobs
uint8_t *jwt_raw(lob_t token); // lob-encoded raw bytes of whole thing
uint32_t jwt_len(lob_t token); // length of raw bytes

lob_t jwt_claims(lob_t token); // just returns the claims from the token
lob_t jwt_verify(lob_t token, e3x_exchange_t x); // checks signature, optional x for pk algs
lob_t jwt_sign(lob_t token, e3x_self_t self); // attaches signature to the claims, optional self for pk algs

// checks if this alg is supported
char *jwt_alg(char *alg);

///////////////////////////////////
// extending support for more JOSE

lob_t jwk_local_get(e3x_self_t self, lob_t jwk, bool private);
e3x_self_t jwk_local_load(lob_t jwk, bool generate);

lob_t jwk_remote_get(e3x_exchange_t x, lob_t jwk);
e3x_exchange_t jwk_remote_load(lob_t jwk);

lob_t jwe_jwt(e3x_exchange_t to, lob_t jwt);
lob_t jwe_decrypt(e3x_self_t self, lob_t jwe, uint8_t *secret);

lob_t jwe_jwt_1c(e3x_exchange_t to, lob_t jwt, uint8_t *ckey);


#endif
