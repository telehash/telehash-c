#ifndef dht_h
#define dht_h

#include "hn.h"

typedef struct dht_struct
{
  hn_t *all;
} *dht_t;

dht_t dht_new();
void dht_free(dht_t d);

void dht_add(dht_t d, hn_t h);

#endif