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

// microseconds for how long the action takes
epoch_t epoch_busy(epoch_t e, uint32_t us)
{
  if(!e) return NULL;
  e->busy = us;
  return e;
}

// when is next knock, returns 0 if == from
uint64_t epoch_knock(epoch_t e, uint64_t from)
{
  return 0;
}

// sync point for given window
epoch_t epoch_sync(epoch_t e, uint32_t window, uint64_t at)
{
  return NULL;
}

// which channel to use at this time
uint32_t epoch_chan(epoch_t e, uint64_t at, uint8_t chans)
{
  return 0;
}


// array utilities
epochs_t epochs_add(epochs_t es, epoch_t e)
{
  size_t j = 0, eslen = epochs_len(es), elen = sizeof(e);
  if(!e) return es;

  // don't double-add
  while(es && es[j])
  {
    if(es[j] == e) return es;
    j++;
  }

  if(!(es = util_reallocf(es, (eslen+2)*elen))) return LOG("OOM");
  es[eslen] = e;
  es[eslen+1] = NULL;
  return es;
}

epochs_t epochs_rem(epochs_t es, epoch_t e)
{
  size_t j = 0, elen = sizeof(e), eslen;
  if(!es) return NULL;
  
  // find it
  while(es[j] && es[j] != e) j++;
  if(!es[j]) return es;
  
  // if alone, zap
  eslen = epochs_len(es);
  if(eslen == 1)
  {
    free(es);
    return NULL;
  }

  // snip out and shrink
  memmove(&es[j], &es[j+1], (eslen-j)*elen); // includes term null in tail
  if(!(es = util_reallocf(es, (eslen*elen)))) return LOG("OOM");
  return es;
}

epoch_t epochs_index(epochs_t es, size_t i)
{
  size_t j = 0;
  if(!es) return NULL;
  while(es[j] && j < i) j++;
  return es[j];
}

size_t epochs_len(epochs_t es)
{
  size_t len = 0;
  if(!es) return 0;
  while(es[len]) len++;
  return len;
}

// free all
epochs_t epochs_free(epochs_t es)
{
  size_t i;
  if(!es) return NULL;
  for(i=0;es[i];i++) epoch_free(es[i]);
  free(es);
  return NULL;
}