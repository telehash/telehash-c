#include "ext.h"

// TODO
// peer_router adds to existing links and registers for link changes to auto-add
// sets its pipe->arg to 1
// change link_handshakes to generate unenc ones w/o a ->x
// send handshakes after any pipe is added to a link


link_t peer_router(link_t peer, link_t router)
{
  lob_t handshakes, hs, open;
  if(!peer || !router) return LOG("bad args");

  // get encrypted handshakes
  if(!(handshakes = link_handshakes(peer)))
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
  
  lob_set_uint(open,"c",0); // so it gets re-set
  lob_set(open,"type","connect");
  link_direct(peer,open,NULL); // encrypts then sends

  return LOG("connect delivered to %s",peer->id->hashname);
}

// direct packets based on type
void peer_send(pipe_t pipe, lob_t packet, link_t link)
{
  if(!pipe || !packet) return;

  // TODO, handshakes become a peer, channels get forwarded
}

pipe_t peer_path(link_t link, lob_t path)
{
  pipe_t pipe, pipes = NULL;
  char *peer;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(util_cmp("peer",lob_get(path,"type"))) return NULL;
  if(!(peer = lob_get(path,"peer"))) return LOG("missing peer");
  if(!hashname_valid(peer)) return LOG("invalid peer");
  
  // get existing one for this peer
  pipes = xht_get(link->mesh->index, "net_tcp4");
  for(pipe = pipes;pipe;pipe = pipe->next) if(util_cmp(pipe->id,peer) == 0) return pipe;

  // make a new one
  if(!(pipe = pipe_new("peer"))) return NULL;
  pipe->id = strdup(peer);
  pipe->send = peer_send;
  pipe->next = pipes;
  xht_set(link->mesh->index,pipe->id,pipe);

  return pipe;
}
