#include <string.h>
#include "net_loopback.h"

void pair_send(pipe_t pipe, lob_t packet, link_t link)
{
  net_loopback_t pair = (net_loopback_t)pipe->arg;
  if(!pair || !packet || !link) return;
  LOG("pair pipe from %s",hashname_short(link->id));
  if(link->mesh == pair->a) mesh_receive(pair->b,packet,pipe);
  else if(link->mesh == pair->b) mesh_receive(pair->a,packet,pipe);
  else lob_free(packet);
}

net_loopback_t net_loopback_new(mesh_t a, mesh_t b)
{
  net_loopback_t pair;

  if(!(pair = malloc(sizeof (struct net_loopback_struct)))) return LOG("OOM");
  memset(pair,0,sizeof (struct net_loopback_struct));
  if(!(pair->pipe = pipe_new("pair")))
  {
    free(pair);
    return LOG("OOM");
  }
  pair->a = a;
  pair->b = b;
  pair->pipe->id = strdup("loopback");
  pair->pipe->arg = pair;
  pair->pipe->send = pair_send;

  // ensure they're linked and piped together
  link_pipe(link_keys(a,b->keys),pair->pipe);
  link_pipe(link_keys(b,a->keys),pair->pipe);

  return pair;
}

void net_loopback_free(net_loopback_t pair)
{
  pipe_free(pair->pipe);
  free(pair);
  return;
}
