#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"
#include "tmesh.h"
#include "chacha.h"


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

// set knock win/chan/start/stop
epoch_t epoch_window(epoch_t e, uint32_t window)
{
  uint8_t pad[8];
  uint8_t nonce[8];
  uint64_t offset;
  uint32_t win;
  if(!e || !e->knock || !e->chans) return LOG("bad args");
  
  // normalize nonce
  memset(nonce,0,8);
  win = util_sys_long(window);
  memcpy(nonce,&win,4);
  
  // ciphertext the pad
  memset(pad,0,8);
  chacha20(e->secret,nonce,pad,8);
  
  e->knock->win = window;
  e->knock->chan = util_sys_short((unsigned short)(pad[0])) % e->chans;
  offset = util_sys_long((unsigned long)(pad[2])) % (EPOCH_WINDOW - e->busy);
  e->knock->start = e->base + (EPOCH_WINDOW*window) + offset;
  e->knock->stop = e->knock->start + e->busy;
  
  return e;
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

// reset active knock to next window, 0 cleans out, guarantees an e->knock or returns NULL
epoch_t epoch_knock(epoch_t e, uint64_t at)
{
  knock_t k;
  uint32_t win;
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
    k->epoch = e;
    memset(k,0,sizeof (struct knock_struct));
    if(!(k->chunks = util_chunks_new(64))) return epoch_knock(e,0);
  }

  // determine current window
  if(at < e->base) at = e->base;
  win = ((at - e->base) / EPOCH_WINDOW);

  // initialize knock win/chan/start/stop
  return epoch_window(e, win+1);
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

