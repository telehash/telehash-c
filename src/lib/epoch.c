#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"
#include "epoch.h"

epoch_t epoch_new(char *id)
{
  epoch_t e;

  if(!(e = malloc(sizeof(struct epoch_struct)))) return LOG("OOM");
  memset(e,0,sizeof (struct epoch_struct));

  if(id && !epoch_import(e, id, NULL))
  {
    LOG("invalid id");
    return epoch_free(e);
  }

  return e;
}

epoch_t epoch_free(epoch_t e)
{
  if(e->id) free(e->id);
  if(e->ext) LOG("epoch free'd leaking external pointer");
  free(e);
  return NULL;
}

epoch_t epoch_import(epoch_t e, char *header, char *body)
{
  if(!e || !(header || body)) return NULL;
  if(header && base32_decode_floor(strlen(header)) < 8) return NULL;
  if(body && base32_decode_floor(strlen(body)) < 8) return NULL;
  base32_decode(header,0,e->bin,16);
  base32_decode(body,0,e->bin+8,8);
  return epoch_reset(e);
}

epoch_t epoch_reset(epoch_t e)
{
  if(!e) return NULL;
//  e3x_hash(e->bin,16,e->pad);
  if(e->id) free(e->id); // free up unused space
  e->id = NULL;
  // convenience pointer into bin
  e->type = e->bin[0];
  return e;
}

char *epoch_id(epoch_t e)
{
  size_t len;
  if(!e) return NULL;
  if(!e->id)
  {
    len = base32_encode_length(16);
    e->id = malloc(len);
    base32_encode(e->bin,16,e->id,len);
  }
  return e->id;
}

// sync point for given window
epoch_t epoch_sync(epoch_t e, uint32_t window, uint64_t at)
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

// init knock to current window of from
knock_t epoch_knock(epoch_t e, knock_t k, uint64_t from)
{
  if(!k || !e || !from) return NULL;

  k->e = e;
  // TODO
  return k;
}

knock_t knock_free(knock_t k)
{
  if(!k) return NULL;
  if(k->buf) free(k->buf);
  free(k);
  return NULL;
}

// microseconds for how long the action takes
epoch_t epoch_busy(epoch_t e, uint32_t us)
{
  if(!e) return NULL;
  e->busy = us;
  return e;
}

// which channel to use at this time
epoch_t epoch_chans(epoch_t e, uint8_t chans)
{
  if(!e) return NULL;
  e->chans = chans;
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