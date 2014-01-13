#include "chans.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

chans_t chans_new()
{
  chans_t l = malloc(sizeof (struct chans_struct));
  bzero(l,sizeof (struct chans_struct));
  l->chans = malloc(sizeof (hn_t));
  l->chans[0] = NULL;
  return l;
}

void chans_free(chans_t l)
{
  free(l->chans);
  free(l);
}

void chans_add(chans_t l, chan_t c)
{
  int i;
  for(i=0;l->chans[i];i++) if(l->chans[i] == c) return;
  l->chans = realloc(l->chans, (i+2) * sizeof(chan_t));
  l->chans[i] = c;
  l->chans[i+1] = NULL;
  l->count = i+1;
}

chan_t chans_get(chans_t l, int index)
{
  if(index >= l->count || index < 0) return NULL;
  return l->chans[index];
}