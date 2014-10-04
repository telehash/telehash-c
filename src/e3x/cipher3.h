#ifndef crypt_h
#define crypt_h

#include "lob.h"

// these are unique to each cipher set implementation
#define local_t (void*)
#define remote_t (void*)
#define ephemeral_t (void*)

// this is the overall holder for each cipher set, function pointers to cs specific implementations
typedef cipher3_struct
{
  // these are common functions each one needs to support
  uint8_t *(*rand)(uint8_t *bytes, uint32_t len); // write len random bytes, returns bytes as well for convenience
  uint8_t *(*hash)(uint8_t *in, uint32_t len, uint8_t *out32); // sha256's the in, out32 must be [32] from caller
  uint8_t *(*err)(void); // last known crypto error string, if any

  // create a new keypair, save encoded to csid in each
  uint8_t (*generate)(lob_t keys, lob_t secrets);

  // our local identity
  local_t (*local_new)(lob_t pair);
  void (*local_free)(local_t local);
  lob_t l(*ocal_decrypt)(local_t local, lob_t outer);
  
  // a remote endpoint identity
  remote_t (*remote_new)(lob_t key);
  void (*remote_free)(remote_t remote);
  uint8_t (*remote_verify)(remote_t remote, local_t local, lob_t outer);
  lob_t (*remote_encrypt)(remote_t remote, local_t local, lob_t inner);
  
  // an active session to a remote for channel packets
  ephemeral_t (*ephemeral_new)(remote_t remote, lob_t outer, lob_t inner);
  void (*ephemeral_free)(ephemeral_t ephemeral);
  lob_t (*ephemeral_encrypt)(ephemeral_t ephemeral, lob_t inner);
  lob_t (*ephemeral_decrypt)(ephemeral_t ephemeral, lob_t outer);
} *cipher3_t;


// all possible cipher sets, as index into cipher_sets global
#define CS_1a 0
#define CS_2a 1
#define CS_3a 2
#define CS_MAX 3;

cipher3_t cipher3_sets[CS_MAX]; // all created
cipher3_t cipher3_default; // just one of them for the rand/hash utils

// calls all cipher3_init_*'s to fill in cipher3_sets[]
uint8_t cipher3_init(lob_t options);

// init functions for each
cipher3_t cs1a_init(lob_t options);
cipher3_t cs2a_init(lob_t options);
cipher3_t cs3a_init(lob_t options);



/*
var local = new cs.Local(pair);
var inner = local.decrypt(body);

var remote = new cs.Remote(public_key_endpoint);
var bool = remote.verify(local, body);
var outer = remote.encrypt(local, inner);

var ephemeral = new cs.Ephemeral(remote, body);
var outer = ephemeral.encrypt(inner)
ver inner = ephemeral.decrypt(outer)
*/

// by default enable CS1a as minimum support
#ifdef NOCS_1a
#else
#define CS_1a
#endif 

typedef struct crypt3_struct *crypt3_t;

// these functions are all independent of CS, implemented in crypt.c

// must be called before any
int crypt_init();

// takes binary or string key format, creates a crypt object
crypt_t crypt_new(char csid, unsigned char *key, int len);
void crypt_free(crypt_t c);


// these exist in the crypt_*_base.c as general crypto routines

// write random bytes, returns s for convenience
unsigned char *crypt_rand(unsigned char *s, int len);

// sha256's the input, output must be [32] from caller
unsigned char *crypt_hash(unsigned char *input, unsigned long len, unsigned char *output);

// last known error for debugging
char *crypt_err();


// the rest of these just use the CS chosen for the crypt_t, crypt.c calls out to crypt_*_XX.c

// adds "XX":"pubkey", "XX_":"secretkey" to the packet, !0 if error
int crypt_keygen(char csid, lob_t p);

// load a private id key, returns !0 if error, can pass (c,NULL,0) to check if private is already loaded too
int crypt_private(crypt_t c, unsigned char *key, int len);

// try to create a line packet chained to this one
lob_t crypt_lineize(crypt_t c, lob_t p);

// decrypts or NULL, frees p
lob_t crypt_delineize(crypt_t c, lob_t p);

// create a new open packet, NULL if error
lob_t crypt_openize(crypt_t self, crypt_t c, lob_t inner);

// processes an open packet into a inner packet or NULL
lob_t crypt_deopenize(crypt_t self, lob_t p);

// tries to create a new line, !0 if error/ignored, always frees inner
int crypt_line(crypt_t c, lob_t inner);

#ifdef CS_1a
int crypt_init_1a();
int crypt_new_1a(crypt_t c, unsigned char *key, int len);
void crypt_free_1a(crypt_t c);
int crypt_keygen_1a(lob_t p);
int crypt_public_1a(crypt_t c, unsigned char *key, int len);
int crypt_private_1a(crypt_t c, unsigned char *key, int len);
lob_t crypt_lineize_1a(crypt_t c, lob_t p);
lob_t crypt_delineize_1a(crypt_t c, lob_t p);
lob_t crypt_openize_1a(crypt_t self, crypt_t c, lob_t inner);
lob_t crypt_deopenize_1a(crypt_t self, lob_t p);
int crypt_line_1a(crypt_t c, lob_t inner);
#endif

#ifdef CS_2a
int crypt_init_2a();
int crypt_new_2a(crypt_t c, unsigned char *key, int len);
void crypt_free_2a(crypt_t c);
int crypt_keygen_2a(lob_t p);
int crypt_public_2a(crypt_t c, unsigned char *key, int len);
int crypt_private_2a(crypt_t c, unsigned char *key, int len);
lob_t crypt_lineize_2a(crypt_t c, lob_t p);
lob_t crypt_delineize_2a(crypt_t c, lob_t p);
lob_t crypt_openize_2a(crypt_t self, crypt_t c, lob_t inner);
lob_t crypt_deopenize_2a(crypt_t self, lob_t p);
int crypt_line_2a(crypt_t c, lob_t inner);
#endif

#ifdef CS_3a
int crypt_init_3a();
int crypt_new_3a(crypt_t c, unsigned char *key, int len);
void crypt_free_3a(crypt_t c);
int crypt_keygen_3a(lob_t p);
int crypt_public_3a(crypt_t c, unsigned char *key, int len);
int crypt_private_3a(crypt_t c, unsigned char *key, int len);
lob_t crypt_lineize_3a(crypt_t c, lob_t p);
lob_t crypt_delineize_3a(crypt_t c, lob_t p);
lob_t crypt_openize_3a(crypt_t self, crypt_t c, lob_t inner);
lob_t crypt_deopenize_3a(crypt_t self, lob_t p);
int crypt_line_3a(crypt_t c, lob_t inner);
#endif

#endif