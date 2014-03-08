#include "bucket.h"
#include <string.h>
#include <stdlib.h>
#include "packet.h"
#include "util.h"
  
bucket_t bucket_new()
{
  bucket_t b = malloc(sizeof (struct bucket_struct));
  memset(b,0,sizeof (struct bucket_struct));
  b->hns = malloc(sizeof (hn_t));
  b->hns[0] = NULL;
  return b;
}

void bucket_free(bucket_t b)
{
  free(b->hns);
  free(b);
}

void bucket_add(bucket_t b, hn_t h)
{
  int i;
  for(i=0;b->hns[i];i++) if(b->hns[i] == h) return;
  b->hns = realloc(b->hns, (i+2) * sizeof(hn_t));
  b->hns[i] = h;
  b->hns[i+1] = NULL;
  b->count = i+1;
}

hn_t bucket_get(bucket_t b, int index)
{
  if(index >= b->count) return NULL;
  return b->hns[index];
}


