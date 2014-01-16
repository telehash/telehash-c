#ifndef crypt_h
#define crypt_h

#include "packet.h"

typedef struct crypt_struct *crypt_t;

// must be called before any
int crypt_init();

// return string of last error
char *crypt_err();

// takes ber or der key format, creates a crypt object
crypt_t crypt_new(unsigned char *key, int len);
void crypt_free(crypt_t c);

// returns 32byte hashname (is safe/stored in c)
unsigned char *crypt_hashname(crypt_t c);

// sets DER value, returns len, caller provides mem
int crypt_der(crypt_t c, unsigned char *der, int len);

// load a private id key, returns !0 if error, can pass (c,NULL,0) to check if private is already loaded too
int crypt_private(crypt_t c, unsigned char *key, int len);

// write random bytes, returns s for convenience
unsigned char *crypt_rand(unsigned char *s, int len);

// try to create a line packet chained to this one
packet_t crypt_lineize(crypt_t self, crypt_t c, packet_t p);

// decrypts or NULL, frees p
packet_t crypt_delineize(crypt_t self, crypt_t c, packet_t p);

// create a new open packet, NULL if error
packet_t crypt_openize(crypt_t self, crypt_t c);

// processes an open packet into a crypt or NULL, frees p
crypt_t crypt_deopenize(crypt_t self, packet_t p);

// merges info from b into a (and frees b)
crypt_t crypt_merge(crypt_t a, crypt_t b);

#endif