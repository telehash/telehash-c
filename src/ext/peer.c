#include "ext.h"

// handle incoming peer request to route
lob_t peer_on_open(link_t link, lob_t open)
{
  link_t peer;
  if(!link) return open;
  if(lob_get_cmp(open,"type","peer")) return open;
  
  LOG("incoming peer route request %s",lob_json(open));

  peer = xht_get(link->mesh->index,lob_get(open,"peer"));
  if(!peer) return LOG("no peer link found");
  
  lob_set(open,"type","connect");
  link_send(peer,open);

  return LOG("connect generated to %s",peer->id->hashname);
}
