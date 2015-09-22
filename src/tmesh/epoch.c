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

mote_t mote_new(link_t link)
{
  mote_t m;
  
  if(!(m = malloc(sizeof(struct mote_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct mote_struct));
  
  if(!(m->chunks = util_chunks_new(64))) return mote_free(m);
  m->link = link;

  return m;
}

mote_t mote_free(mote_t m)
{
  if(!m) return NULL;
  util_chunks_free(m->chunks);
  free(m);
  return NULL;
}

// set best knock win/chan/start/stop
mote_t mote_knock(mote_t m, medium_t medium, uint64_t from)
{
  uint8_t pad[8];
  uint8_t nonce[8];
  uint64_t offset, start, stop;
  uint32_t win, lwin;
  epoch_t e;
  if(!m || !medium->chans) return LOG("bad args");

  m->knock = NULL;

  // get the best one
  for(e=m->epochs;e;e=e->next)
  {

    // normalize nonce
    memset(nonce,0,8);
    switch(e->type)
    {
      case PING:
        win = 0;
        e->base = from; // is always now
        break;
      case ECHO:
        win = 1;
        break;
      case PAIR:
      case LINK:
        win = ((from - e->base) / EPOCH_WINDOW);
        break;
      default :
        continue;
    }
    if(!e->base) continue;

    lwin = util_sys_long(win);
    memcpy(nonce,&lwin,4);
  
    // ciphertext the pad
    memset(pad,0,8);
    chacha20(e->secret,nonce,pad,8);
    
    offset = util_sys_long((unsigned long)(pad[2])) % (EPOCH_WINDOW - medium->min);
    start = e->base + (EPOCH_WINDOW*win) + offset;
    stop = start + medium->min;
    if(!m->knock || stop < m->kstop)
    {
      // rx never trumps tx
      if(m->knock && m->knock->dir == TX && e->dir == RX) continue;
      m->knock = e;
      m->kchan = util_sys_short((unsigned short)(pad[0])) % medium->chans;
      m->kstart = start;
      m->kstop = stop;
    }
  }
  
  if(m->knock) m->kstate = READY;
  
  return m;
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

