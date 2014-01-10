#ifndef crypt_h
#define crypt_h

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

// load a private id key, returns !0 if error, can pass (c,NULL,0) to check if private is already loaded too
int crypt_private(crypt_t c, unsigned char *key, int len);

#endif