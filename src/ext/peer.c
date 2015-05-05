#include "ext.h"

link_t peer_router(link_t peer, link_t router)
{
  lob_t handshakes, hs, open;
  if(!peer || !router) return LOG("bad args");

  // get encrypted handshakes
  if(!(handshakes = link_sync(peer)))
  {
    LOG("can't get encrypted handshakes, TODO create bare ones (per CSID)");
    return NULL;
  }
  
  // loop through and send each one in a peer request through the router
  for(hs = handshakes; hs; hs = lob_linked(hs))
  {
    open = lob_new();
    lob_set(open,"type","peer");
    lob_set(open,"peer",peer->id->hashname);
    lob_body(open,lob_raw(hs),lob_len(hs));
    link_direct(router,open,NULL);
    lob_free(open);
  }
  
  lob_free(handshakes);

  return peer;
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
