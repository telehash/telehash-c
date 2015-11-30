#include <stddef.h>

#ifndef xht_h
#define xht_h

// simple string->void* hashtable, very static and bare minimal, but efficient

typedef struct xht_struct *xht_t;

// must pass a prime#
xht_t xht_new(unsigned int prime);

// caller responsible for key storage, no copies made (don't free it b4 xht_free()!)
// set val to NULL to clear an entry, memory is reused but never free'd (# of keys only grows to peak usage)
void xht_set(xht_t h, const char *key, void *val);

// ooh! unlike set where key/val is in caller's mem, here they are copied into xht_t and free'd when val is 0 or xht_free()
void xht_store(xht_t h, const char *key, void *val, size_t vlen);

// returns value of val if found, or NULL
void *xht_get(xht_t h, const char *key);

// free the hashtable and all entries
void xht_free(xht_t h);

// pass a function that is called for every key that has a value set
typedef void (*xht_walker)(xht_t h, const char *key, void *val, void *arg);
void xht_walk(xht_t h, xht_walker w, void *arg);

// iterator through all the keys (NULL to start), use get for values
char *xht_iter(xht_t h, char *key);

#endif

