#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"
#include "epoch.h"

epoch_t epoch_new(uint8_t medium[6])
{
  epoch_t e;

  // TODO medium driver init

  if(!(e = malloc(sizeof(struct epoch_struct)))) return LOG("OOM");
  memset(e,0,sizeof (struct epoch_struct));
  
  e->type = medium[0];

  return e;
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

// make a new knock
knock_t knock_new(uint8_t tx)
{
  knock_t k;

  if(!(k = malloc(sizeof(struct knock_struct)))) return LOG("OOM");
  memset(k,0,sizeof (struct knock_struct));
  k->tx = tx;
  return k;
}

// reset active knock to next window, 0 cleans out, guarantees an e->knock or returns NULL
epoch_t epoch_knock(epoch_t e, uint64_t at)
{
  if(!e) return NULL;

  // TODO
  return e;
}

knock_t knock_free(knock_t k)
{
  if(!k) return NULL;
  if(k->buf) free(k->buf);
  free(k);
  return NULL;
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

// all drivers
epoch_medium_t epoch_mediums[EPOCH_MEDIUMS_MAX];

epoch_medium_t epoch_medium_set(uint8_t type, epoch_medium_t driver)
{
  return NULL;
}

epoch_medium_t epoch_medium_get(uint8_t type)
{
  return NULL;
}
