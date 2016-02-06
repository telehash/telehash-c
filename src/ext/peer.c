#include <string.h>
#include "telehash.h"

// direct packets based on type
void peer_send(pipe_t pipe, lob_t packet, link_t link)
{
  lob_t open;
  link_t router;
  if(!pipe || !packet || !link) return;

  if(!(router = mesh_linkid(link->mesh, pipe->arg)))
  {
    LOG("router link not found for %s",pipe->id);
    return;
  }

  // encrypted channel packets get sent directly for bridging
  if(packet->head_len == 0)
  {
    LOG("bridging via router %s",pipe->id);
    link_send(router, packet);
    return;
  }
  
  LOG("peering via router %s",pipe->id);
  open = lob_new();
  lob_set(open,"type","peer");
  lob_set(open,"peer",hashname_char(link->id));
  lob_body(open,lob_raw(packet),lob_len(packet));
  link_direct(router,open,NULL);

  // store a direct route back based on the token in this open
  mesh_forward(link->mesh, packet->body, link, 0);
  lob_free(packet);
  
}

pipe_t peer_pipe(mesh_t mesh, hashname_t peer)
{
  char *sn = hashname_short(peer);
  pipe_t pipe, pipes = NULL;

  // get existing one for this peer
  pipes = xht_get(mesh->index, "ext_peer_pipes");
  for(pipe = pipes;pipe;pipe = pipe->next) if(util_cmp(pipe->id,sn) == 0) return pipe;

  // make a new one
  if(!(pipe = pipe_new("peer"))) return NULL;
  pipe->id = strdup(hashname_short(peer));
  pipe->arg = hashname_dup(peer);
  pipe->send = peer_send;
  pipe->next = pipes;
  xht_set(mesh->index,"ext_peer_pipes",pipe);

  return pipe;
}

pipe_t peer_path(link_t link, lob_t path)
{
  char *peer;
  hashname_t id;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(util_cmp("peer",lob_get(path,"type"))) return NULL;
  if(!(peer = lob_get(path,"peer"))) return LOG("missing peer");
  if(!(id = hashname_vchar(peer))) return LOG("invalid peer");
  
  return peer_pipe(link->mesh, id);
}

// handle an incoming connect request
lob_t peer_open_connect(link_t link, lob_t open)
{
  pipe_t pipe = NULL;
  lob_t hs;
  char *hn = lob_get(open,"peer");
  if(!link || lob_get_cmp(open,"type","connect") != 0) return open;
  if(!hn || !(hs = lob_parse(open->body,open->body_len)))
  {
    LOG("invalid peer request");
    return lob_free(open);
  }

  LOG("incoming connect for %s via %s",hn,hashname_short(link->id));
  
  // get routing peer pipe
  pipe = peer_pipe(link->mesh, link->id);


  // encrypted or not handshake
  if(hs->head_len == 1) mesh_receive(link->mesh, hs, pipe);
  else{
    lob_set(hs,"id","connect");
    mesh_receive_handshake(link->mesh, hs, pipe);
  }

  return lob_free(open);
}

// handle incoming peer request to route
lob_t peer_open_peer(link_t link, lob_t open)
{
  link_t peer;
  if(!link) return open;
  if(lob_get_cmp(open,"type","peer")) return open;

  LOG("incoming peer route request %s",lob_json(open));

  if(!(peer = mesh_linkid(link->mesh,hashname_vchar(lob_get(open,"peer")))))
  {
    LOG("no peer link found for %s from %s",lob_get(open,"peer"),hashname_short(link->id));
    return open;
  }
  
  lob_t packet;
  if(!(packet = lob_parse(open->body,open->body_len)))
  {
    LOG("invalid peer request body: %s",util_hex(open->body,open->body_len,NULL));
    return open;
  }
  // set up forwarding if a handshake is observed
  if(packet->head_len == 1)
  {
    uint8_t hash[32];
    e3x_hash(packet->body,16,hash);
    mesh_forward(link->mesh, hash, link, 0);
  }
  lob_free(packet);
  
  
  lob_set_uint(open,"c",0); // so it gets re-set
  lob_set(open,"type","connect");
  link_direct(peer,open,NULL); // encrypts then sends

  return LOG("connect delivered to %s",hashname_short(peer->id));
}

void peer_free(mesh_t mesh)
{
  pipe_t pipes, pipe;
  pipes = xht_get(mesh->index, "ext_peer_pipes");
  while(pipes)
  {
    pipe = pipes;
    pipes = pipe->next;
    hashname_free(pipe->arg);
    pipe_free(pipe);
  }
}

// enables peer/connect handling for this mesh
mesh_t peer_enable(mesh_t mesh)
{
  mesh_on_open(mesh, "ext_peer", peer_open_connect);
  mesh_on_path(mesh, "ext_peer", peer_path);
  mesh_on_free(mesh, "ext_peer", peer_free);
  return mesh;
}

// enables routing for this mesh
mesh_t peer_route(mesh_t mesh)
{
  mesh_on_open(mesh, "ext_peer_route", peer_open_peer);
  return mesh;
}

// adds default router
link_t peer_router(link_t router)
{
  // TODO
  // get a pipe to this router and set it's default flag
  // register to receive link events
  // loop through existing links and add our pipe
  return NULL;
}

// try to connect this peer via this router (sends an ad-hoc peer request)
link_t peer_connect(link_t peer, link_t router)
{
  lob_t handshakes, hs;
  pipe_t pipe;
  if(!peer || !router) return LOG("bad args");

  // get router pipe and handshakes
  if(!(pipe = peer_pipe(peer->mesh, router->id)) || !(handshakes = link_handshakes(peer))) return LOG("internal error");
  
  // loop through and send each one in a peer request through the router
  for(hs = handshakes; hs; hs = handshakes)
  {
    handshakes = lob_linked(hs);
    peer_send(pipe, hs, peer);
  }
  
  return peer;
}
