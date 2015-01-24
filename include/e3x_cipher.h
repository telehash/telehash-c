#ifndef e3x_cipher_h
#define e3x_cipher_h

#include "lob.h"

// these are unique to each cipher set implementation
#define local_t void*
#define remote_t void*
#define ephemeral_t void*

// this is the overall holder for each cipher set, function pointers to cs specific implementations
typedef struct e3x_cipher_struct
{
  uint8_t id, csid;
  char hex[3], *alg;

  // these are common functions each one needs to support
  uint8_t *(*rand)(uint8_t *bytes, size_t len); // write len random bytes, returns bytes as well for convenience
  uint8_t *(*hash)(uint8_t *in, size_t len, uint8_t *out32); // sha256's the in, out32 must be [32] from caller
  uint8_t *(*err)(void); // last known crypto error string, if any

  // create a new keypair, save encoded to csid in each
  uint8_t (*generate)(lob_t keys, lob_t secrets);

  // our local identity
  local_t (*local_new)(lob_t keys, lob_t secrets);
  void (*local_free)(local_t local);
  lob_t (*local_decrypt)(local_t local, lob_t outer);
  lob_t (*local_sign)(local_t local, lob_t args, uint8_t *data, size_t len);
  
  // a remote endpoint identity
  remote_t (*remote_new)(lob_t key, uint8_t *token);
  void (*remote_free)(remote_t remote);
  uint8_t (*remote_verify)(remote_t remote, local_t local, lob_t outer);
  lob_t (*remote_encrypt)(remote_t remote, local_t local, lob_t inner);
  uint8_t (*remote_validate)(remote_t remote, lob_t args, lob_t sig, uint8_t *data, size_t len);
  
  // an active session to a remote for channel packets
  ephemeral_t (*ephemeral_new)(remote_t remote, lob_t outer);
  void (*ephemeral_free)(ephemeral_t ephemeral);
  lob_t (*ephemeral_encrypt)(ephemeral_t ephemeral, lob_t inner);
  lob_t (*ephemeral_decrypt)(ephemeral_t ephemeral, lob_t outer);
} *e3x_cipher_t;


// all possible cipher sets, as index into cipher_sets global
#define CS_1a 0
#define CS_2a 1
#define CS_3a 2
#define CS_MAX 3

extern e3x_cipher_t e3x_cipher_sets[]; // all created
extern e3x_cipher_t e3x_cipher_default; // just one of them for the rand/hash utils

// calls all e3x_cipher_init_*'s to fill in e3x_cipher_sets[]
uint8_t e3x_cipher_init(lob_t options);

// return by id or hex
e3x_cipher_t e3x_cipher_set(uint8_t csid, char *hex);

// init functions for each
e3x_cipher_t cs1a_init(lob_t options);
e3x_cipher_t cs2a_init(lob_t options);
e3x_cipher_t cs3a_init(lob_t options);

#endif
