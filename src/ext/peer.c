#include "ext.h"

link_t peer_router(link_t peer, link_t router)
{
  if(!peer || !router) return LOG("bad args");

  // TODO compose best open for peer
  return NULL;
}

// handle incoming peer request to route
lob_t peer_on_open(link_t link, lob_t open)
{
  link_t peer;
  if(!link) return open;
  if(lob_get_cmp(open,"type","peer")) return open;

  LOG("incoming peer route request %s",lob_json(open));

  if(!(peer = mesh_linked(link->mesh,lob_get(open,"peer"))))
  {
    LOG("no peer link found for %s from %s",lob_get(open,"peer"),link->id->hashname);
    return open;
  }
  
  lob_set(open,"type","connect");
  link_send(peer,open);

  return LOG("connect generated to %s",peer->id->hashname);
}
