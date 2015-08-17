#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"
#include "tmesh_epoch.h"

epoch_t epoch_new(mesh_t m, uint8_t medium[6])
{
  epoch_t e;
  int i;
  
  if(!medium) return NULL;

  if(!(e = malloc(sizeof(struct epoch_struct)))) return LOG("OOM");
  memset(e,0,sizeof (struct epoch_struct));
  memcpy(e->medium,medium,6);

  // tell drivers to initialize until one does
  for(i=0;i<EPOCH_DRIVERS_MAX && epoch_drivers[i];i++)
  {
    if((e->driver = epoch_drivers[i]->init(m,e,medium))) return e;
  }
  
  LOG("no driver for medium %s",epoch_medium(e));
  return epoch_free(e);
}

epoch_t epoch_free(epoch_t e)
{
  epoch_knock(e,0); // free's knock
  // TODO medium driver free
  free(e);
  return NULL;
}

static char epoch_mcache[11];
char *epoch_medium(epoch_t e)
{
  if(!e) return NULL;
  base32_encode(e->medium,6,epoch_mcache,11);
  return (char*)epoch_mcache;
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
    if(e->knock->buf) free(e->knock->buf);
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

// all drivers
epoch_driver_t epoch_drivers[EPOCH_DRIVERS_MAX] = {0};

epoch_driver_t epoch_driver(epoch_driver_t driver)
{
  int i;
  for(i=0;i<EPOCH_DRIVERS_MAX;i++)
  {
    if(epoch_drivers[i]) continue;
    epoch_drivers[i] = driver;
    return driver;
  }
  return NULL;
}
