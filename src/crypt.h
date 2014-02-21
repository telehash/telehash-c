#ifndef crypt_h
#define crypt_h

#include "packet.h"

typedef struct crypt_struct
{
  char csid, csidHex[3], *part;
  int private, lined, keylen;
  unsigned long atOut, atIn;
  unsigned char lineOut[16], lineIn[16], lineHex[33];
  unsigned char *key;
  void *cs; // for CS usage
} *crypt_t;

char crypt_supported[8] = "\0\0\0\0\0\0\0\0";

// these functions are all independent of CS, implemented in crypt.c

// must be called before any
int crypt_init();

// takes ber or der key format, creates a crypt object
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

// copies in the current binary public key value, returns len, caller provides key mem
int crypt_public(crypt_t c, unsigned char *key, int len);

// load a private id key, returns !0 if error, can pass (c,NULL,0) to check if private is already loaded too
int crypt_private(crypt_t c, unsigned char *key, int len);

// try to create a line packet chained to this one
packet_t crypt_lineize(crypt_t self, crypt_t c, packet_t p);

// decrypts or NULL, frees p
packet_t crypt_delineize(crypt_t self, crypt_t c, packet_t p);

// create a new open packet, NULL if error
packet_t crypt_openize(crypt_t self, crypt_t c, packet_t inner);

// processes an open packet into a inner packet or NULL, frees p if successful
packet_t crypt_deopenize(crypt_t self, packet_t p);

// merges info from b into a (and frees b), !0 if error/ignored, always frees inner
int crypt_open(crypt_t c, packet_t inner);

#ifdef CS_2a
int crypt_init_2a();
int crypt_new_2a(crypt_t c, unsigned char *key, int len);
void crypt_free_2a(crypt_t c);
int crypt_public_2a(crypt_t c, unsigned char *key, int len);
int crypt_private_2a(crypt_t c, unsigned char *key, int len);
int crypt_lineinit_2a(crypt_t c);
packet_t crypt_lineize_2a(crypt_t self, crypt_t c, packet_t p);
packet_t crypt_delineize_2a(crypt_t self, crypt_t c, packet_t p);
packet_t crypt_openize_2a(crypt_t self, crypt_t c, packet_t inner);
packet_t crypt_deopenize_2a(crypt_t self, packet_t p);
int crypt_open_2a(crypt_t c, packet_t inner);
#endif

#endif