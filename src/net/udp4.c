#include <string.h>
#include "udp4.h"

#define ONID "net_udp4"

pipe_t udp4_path(link_t link, lob_t path)
{
  net_udp4_t udp4;
  if(!link || !path) return NULL;
  if(!(udp4 = xht_get(link->mesh->index, ONID))) return NULL;
  if(util_cmp("udp4",lob_get(path,"type"))) return NULL;
}

net_udp4_t net_udp4_new(mesh_t a, mesh_t b)
{
  net_udp4_t udp4;

  if(!(udp4 = malloc(sizeof (struct net_udp4_struct)))) return LOG("OOM");
  memset(udp4,0,sizeof (struct net_udp4_struct));
  udp4->a = a;
  udp4->b = b;
  
  
  return udp4;
}

void net_udp4_free(net_udp4_t udp4)
{
  free(udp4);
  return;
}
