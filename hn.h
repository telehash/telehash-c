#ifndef hn_h
#define hn_h

#include "xht.h"

// a prime number for the hashtable used to track all active hashnames
#define HNMAXPRIME 4211

typedef struct hn_struct
{
  const char hashname[32];
} *hn_t;

// global index of all hashname structures
xht hn_ndx;

// initialize hashname handling
void hn_init();

// call at least once a minute to clean up old hashnames
void hn_gc();

// fetch/create matching hn (binary or hex)
hn_t hn_get32(char *hn);
hn_t hn_get64(char *hn);

#endif