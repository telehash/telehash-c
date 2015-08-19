#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"
#include "tmesh.h"

epoch_t epoch_new(mesh_t m, uint8_t medium[6])
{
  epoch_t e;
  int i;
  
  if(!medium) return NULL;

  if(!(e = malloc(sizeof(struct epoch_struct)))) return LOG("OOM");
  memset(e,0,sizeof (struct epoch_struct));

  // tell devices to bind until one does
  for(i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if((e->device = radio_devices[i]->bind(m,e,medium))) return e;
  }
  
  LOG("no device for this medium");
  return epoch_free(e);
}

epoch_t epoch_free(epoch_t e)
{
  epoch_knock(e,0); // free's knock
  // TODO medium driver free
  free(e);
  return NULL;
}

// sync point for given window
epoch_t epoch_base(epoch_t e, uint32_t window, uint64_t at)
{
  return NULL;
}

// reset active knock to next window, 0 cleans out, guarantees an e->knock or returns NULL
epoch_t epoch_knock(epoch_t e, uint64_t at)
{
  knock_t k;
  if(!e) return NULL;
  
  // free knock
  if(!at)
  {
    if(!e->knock) return NULL;
    if(e->knock->chunks) util_chunks_free(e->knock->chunks);
    free(e->knock);
    e->knock = NULL;
    return NULL;
  }

  // init space for one
  if(!e->knock)
  {
    if(!(k = malloc(sizeof(struct knock_struct)))) return LOG("OOM");
    e->knock = k;
    memset(k,0,sizeof (struct knock_struct));
  }

  // TODO initialize knock win/chan/start/stop

  return e;
}

// array utilities
epoch_t epochs_add(epoch_t es, epoch_t e)
{
  epoch_t i;
  if(!es) return e;
  if(!e) return es;
  // dedup
  for(i=es;i;i = i->next) if(i == e) return es;
  e->next = es;
  return e;
}

epoch_t epochs_rem(epoch_t es, epoch_t e)
{
  epoch_t i;
  if(!es) return NULL;
  if(!e) return es;
  if(es == e) return e->next;
  for(i=es;i;i = i->next)
  {
    if(i->next == e) i->next = e->next;
  }
  return es;
}

size_t epochs_len(epoch_t es)
{
  epoch_t i;
  size_t len = 0;
  for(i=es;i;i = i->next) len++;
  return len;
}

// free all, tail recurse
epoch_t epochs_free(epoch_t es)
{
  epoch_t e;
  if(!es) return NULL;
  e = es->next;
  epoch_free(es);
  return epochs_free(e);
}

