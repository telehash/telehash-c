#include "chans.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

chans_t chans_new()
{
  chans_t l = malloc(sizeof (struct chans_struct));
  bzero(l,sizeof (struct chans_struct));
  return l;
}

void chans_free(chans_t l)
{
  if(l->chans) free(l->chans);
  free(l);
}

void chans_add(chans_t l, chan_t c)
{
  int i;
  for(i=0;i<l->count;i++)
  {
    if(l->chans[i] == c) return;
    if(l->chans[i]) continue;
    // fill in an empty slot
    l->chans[i] = c;
    return;
  }
  // add a new slot
  l->chans = realloc(l->chans, (l->count+1) * sizeof(chan_t));
  l->chans[l->count] = c;
  l->count++;
}

chan_t chans_pop(chans_t l)
{
  int i;
  chan_t c;

  if(!l) return NULL;
  for(i=0;i<l->count;i++) if(l->chans[i])
  {
    c = l->chans[i];
    l->chans[i] = NULL;
    return c;
  }
  return NULL;
}
