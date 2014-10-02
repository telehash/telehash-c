#ifndef crypt_h
#define crypt_h

#include "lob.h"

// by default enable CS1a as minimum support
#ifdef NOCS_1a
#else
#define CS_1a
#endif 

typedef struct crypt_struct
{
  char csidHex[3], *part;
  int isprivate, lined, keylen;
  unsigned long atOut, atIn;
  unsigned char lineOut[16], lineIn[16], lineHex[33];
  unsigned char *key, csid;
  void *cs; // for CS usage
} *crypt_t;

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