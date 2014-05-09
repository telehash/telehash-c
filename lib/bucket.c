#include "bucket.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "packet.h"
#include "util.h"

bucket_t bucket_new()
{
  bucket_t b = malloc(sizeof (struct bucket_struct));
  memset(b,0,sizeof (struct bucket_struct));
  b->hns = NULL;
  return b;
}

void bucket_free(bucket_t b)
{
  if(!b) return;
  if(b->hns) free(b->hns);
  free(b);
}

void bucket_add(bucket_t b, hn_t hn)
{
  int i;
  void *ptr;
  if(!b || !hn) return;
  for(i=0;i<b->count;i++) if(b->hns[i] == hn) return; // already here
  // make more space
  if(!(ptr = realloc(b->hns, (b->count+1) * sizeof(hn_t)))) return;
  b->hns = (hn_t*)ptr;
  b->hns[b->count] = hn;
  b->count++;
}

void bucket_rem(bucket_t b, hn_t hn)
{
  int i, at = -1;
  if(!b || !hn) return;
  for(i=0;i<b->count;i++) if(b->hns[i] == hn) at = i;
  if(at == -1) return;
  b->count--;
  memmove(b->hns+at,b->hns+(at+1),b->count-at);
}

hn_t bucket_get(bucket_t b, int index)
{
  if(!b || index >= b->count) return NULL;
  return b->hns[index];
}


