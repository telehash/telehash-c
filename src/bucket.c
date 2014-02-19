#include "bucket.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "packet.h"
#include "util.h"
  
bucket_t bucket_new()
{
  bucket_t b = malloc(sizeof (struct bucket_struct));
  bzero(b,sizeof (struct bucket_struct));
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

bucket_t bucket_load(xht_t index, char *file)
{
  packet_t p, p2;
  hn_t hn;
  bucket_t b = NULL;
  int i;

  p = util_file2packet(file);
  if(!p) return b;
  if(*p->json != '{')
  {
    packet_free(p);
    return b;
  }

  // check each value, since js0n is key,len,value,len,key,len,value,len for objects
	for(i=0;p->js[i];i+=4)
	{
    p2 = packet_new();
    packet_json(p2, p->json+p->js[i+2], p->js[i+3]-p->js[i+2]);
    hn = hn_getjs(index, p2);
    packet_free(p2);
    if(!hn) continue;
    if(!b) b = bucket_new();
    bucket_add(b, hn);
	}

  packet_free(p);
  return b;
}
