#include <string.h>
#include "net_loopback.h"

link_t pair_send(link_t link, lob_t packet, void *arg)
{
  net_loopback_t pair = (net_loopback_t)arg;
  if(!pair || !packet || !link) return link;
  LOG("pair pipe from %s",hashname_short(link->id));
  if(link->mesh == pair->a) mesh_receive(pair->b,packet);
  else if(link->mesh == pair->b) mesh_receive(pair->a,packet);
  else lob_free(packet);
  return link;
}

net_loopback_t net_loopback_new(mesh_t a, mesh_t b)
{
  net_loopback_t pair;

  if(!(pair = malloc(sizeof (struct net_loopback_struct)))) return LOG("OOM");
  memset(pair,0,sizeof (struct net_loopback_struct));
  pair->a = a;
  pair->b = b;

  // ensure they're linked and piped together
  link_pipe(link_keys(a,b->keys),pair_send,pair);
  link_pipe(link_keys(b,a->keys),pair_send,pair);

  return pair;
}

void net_loopback_free(net_loopback_t pair)
{
  free(pair);
  return;
}
