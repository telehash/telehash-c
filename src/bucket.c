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
  b->args = NULL;
  return b;
}

void bucket_free(bucket_t b)
{
  if(!b) return;
  if(b->hns) free(b->hns);
  if(b->args) free(b->args);
  free(b);
}

void bucket_add(bucket_t b, hn_t hn)
{
  void *ptr;
  if(!b || !hn) return;
  // already here?
  if(bucket_in(b,hn) >= 0) return;
  // make more space, args too if any
  if(b->args)
  {
    if(!(ptr = realloc(b->args, (b->count+1) * sizeof(void*)))) return;
    b->args = (void**)ptr;
    b->args[b->count] = NULL;
  }
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
  if(b->args) memmove(b->args+at,b->args+(at+1),b->count-at);
}

hn_t bucket_get(bucket_t b, int index)
{
  if(!b || index >= b->count) return NULL;
  return b->hns[index];
}

int bucket_in(bucket_t b, hn_t hn)
{
  int i;
  if(!b || !hn) return -1;
  for(i=0;i<b->count;i++) if(b->hns[i] == hn) return i;
  return -1;
}

// these set and return an optional arg for the matching hashname
void bucket_set(bucket_t b, hn_t hn, void *arg)
{
  int i;
  bucket_add(b,hn);
  i = bucket_in(b,hn);
  if(i < 0) return;
  if(!b->args)
  {
    b->args = malloc(b->count * sizeof(void*));
    if(!b->args) return;
    memset(b->args,0,b->count * sizeof (void*));
  }
  b->args[i] = arg;
}

void *bucket_arg(bucket_t b, hn_t hn)
{
  int i = bucket_in(b,hn);
  if(i < 0 || !b->args) return NULL;
  return b->args[i];
}

