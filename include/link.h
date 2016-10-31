#ifndef link_h
#define link_h
#include <stdint.h>

#include "mesh.h"

struct link_struct
{
  // public link data
  hashname_t id;
  e3x_exchange_t x;
  mesh_t mesh;
  lob_t key, handshake;
  chan_t chans;

  // transport plumbing
  void *send_arg;
  link_t (*send_cb)(link_t link, lob_t packet, void *arg);
  
  // these are for internal link management only
  link_t next;
  uint8_t csid;
};

// these all create or return existing one from the mesh
link_t link_get(mesh_t mesh, hashname_t id);
link_t link_get_keys(mesh_t mesh, lob_t keys); // adds in the right key
link_t link_get_key(mesh_t mesh, lob_t key, uint8_t csid); // adds in from the body

// simple accessors
hashname_t link_id(link_t link);
lob_t link_key(link_t link);

// get link info json
lob_t link_json(link_t link);

// removes from mesh
void link_free(link_t link);

// load in the key to existing link
link_t link_load(link_t link, uint8_t csid, lob_t key);

// add a delivery pipe to this link
link_t link_pipe(link_t link, link_t (*send)(link_t link, lob_t packet, void *arg), void *arg);

// process a decrypted channel packet
link_t link_receive(link_t link, lob_t inner);

// process an incoming handshake
link_t link_receive_handshake(link_t link, lob_t handshake);

// try to deliver this encrypted packet
link_t link_send(link_t link, lob_t outer);

// encrypt and send this packet
link_t link_direct(link_t link, lob_t inner);

// return current handshake (caller free's)
lob_t link_handshake(link_t link);

// send current handshake(s) 
link_t link_sync(link_t link);

// force generate new encrypted handshake(s) and sync
link_t link_resync(link_t link);

// is the other endpoint connected and the link available, NULL if not
link_t link_up(link_t link);

// force link down, ends channels and generates events
link_t link_down(link_t link);

// create/track a new channel for this open
chan_t link_chan(link_t link, lob_t open);

// process any channel timeouts based on the current/given time
link_t link_process(link_t link, uint32_t now);

#endif
