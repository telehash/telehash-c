#include <string.h>

#include "pair.h"

net_pair_t net_pair_new(mesh_t a, mesh_t b)
{
  net_pair_t pair;

  if(!(pair = malloc(sizeof (struct net_pair_struct)))) return LOG("OOM");
  memset(pair,0,sizeof (struct net_pair_struct));
  pair->a = a;
  pair->b = b;
  return pair;
}

void net_pair_free(net_pair_t pair)
{
  free(pair);
  return;
}
