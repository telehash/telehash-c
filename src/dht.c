#include "dht.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
  
dht_t dht_new()
{
  dht_t d = malloc(sizeof (struct dht_struct));
  bzero(d,sizeof (struct dht_struct));
  d->all = malloc(sizeof (hn_t));
  d->all[0] = NULL;
  return d;
}

void dht_free(dht_t d)
{
  free(d->all);
  free(d);
}

void dht_add(dht_t d, hn_t h)
{
  int i;
  for(i=0;d->all[i];i++) if(d->all[i] == h) return;
  d->all = realloc(d->all, (i+2) * sizeof(hn_t));
  d->all[i] = h;
  d->all[i+1] = NULL;
}