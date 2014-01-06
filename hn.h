#ifndef hn_h
#define hn_h

#include "crypt.h"
#include "xht.h"

// a prime number for the internal hashtable used to track all active hashnames
#define HNMAXPRIME 4211

typedef struct hn_struct
{
  unsigned char hashname[32];
  crypt_t c;
} *hn_t;

// global index of all hashname structures
xht _hn_index;

// initialize hashname handling
void hn_init();

// call at least once a minute to clean up old hashnames
void hn_gc();

// fetch/create matching hn (binary or hex)
hn_t hn_get(unsigned char *hn);
hn_t hn_gethex(char *hex);

// load hashname from file
hn_t hn_idfile(char *file);

// load hashnames from a seeds file and return them, caller must free return array
hn_t *hn_seeds(char *file);

#endif