#include <string.h>
#include "pair.h"

#define ONID "net_pair"

void pair_send(pipe_t pipe, lob_t packet, link_t link)
{
  net_pair_t pair = (net_pair_t)pipe->arg;
  if(!pair || !packet || !link) return;
  LOG("pair pipe from %s",link->id->hashname);
  if(link->mesh == pair->a) mesh_receive(pair->b,packet,pipe);
  if(link->mesh == pair->b) mesh_receive(pair->a,packet,pipe);
}

net_pair_t net_pair_new(mesh_t a, mesh_t b)
{
  net_pair_t pair;

  if(!(pair = malloc(sizeof (struct net_pair_struct)))) return LOG("OOM");
  memset(pair,0,sizeof (struct net_pair_struct));
  pair->a = a;
  pair->b = b;
  pair->pipe = pipe_new("pair");
  pair->pipe->arg = pair;
  pair->pipe->send = pair_send;

  // ensure they're linked and piped together
  link_pipe(link_keys(a,b->keys),pair->pipe);
  link_pipe(link_keys(b,a->keys),pair->pipe);

  return pair;
}

void net_pair_free(net_pair_t pair)
{
  pipe_free(pair->pipe);
  free(pair);
  return;
}
