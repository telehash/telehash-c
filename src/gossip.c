#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "telehash.h"

gossip_t gossip_new(void)
{
  gossip_t ret = malloc(sizeof(gossip_s));
  memset(ret,0,sizeof(gossip_s));
  return ret;
}

gossip_t gossip_free(gossip_t g)
{
  if(!g) return NULL;
  if(g->next) g->next = gossip_free(g->next);
  free(g);
  return NULL;
}
