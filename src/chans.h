#ifndef chans_h
#define chans_h

#include "chan.h"

typedef struct chans_struct
{
  int count;
  chan_t *chans;
} *chans_t;

chans_t chans_new();
void chans_free(chans_t l);

void chans_add(chans_t l, chan_t c);

// simple array index function
chan_t chans_get(chans_t l, int index);


#endif