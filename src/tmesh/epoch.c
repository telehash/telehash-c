#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "telehash.h"
#include "tmesh.h"


epoch_t epoch_new(uint8_t tx)
{
  epoch_t e;
  
  if(!(e = malloc(sizeof(struct epoch_struct)))) return LOG("OOM");
  memset(e,0,sizeof (struct epoch_struct));
  e->dir = (tx)?TX:RX;

  return e;
}

epoch_t epoch_free(epoch_t e)
{
  free(e);
  return NULL;
}


// sync point for given window
epoch_t epoch_base(epoch_t e, uint32_t window, uint64_t at)
{
  uint64_t base;
  if(!e) return LOG("bad args");
  base = window * EPOCH_WINDOW;
  if(base > at) return LOG("bad window"); 
  e->base = at - base;
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

