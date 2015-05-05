#ifndef link_h
#define link_h
#include <stdint.h>

typedef struct link_struct *link_t;

#include "mesh.h"

struct link_struct
{
  // public link data
  hashname_t id;
  e3x_exchange_t x;
  mesh_t mesh;
  lob_t key;
  lob_t handshakes;
  uint8_t csid;
  xht_t index, channels;
  char token[33];
  
  // these are for internal link management only
  struct seen_struct *pipes;
};

// these all create or return existing one from the mesh
link_t link_get(mesh_t mesh, char *hashname);
link_t link_keys(mesh_t mesh, lob_t keys); // adds in the right key
link_t link_key(mesh_t mesh, lob_t key, uint8_t csid); // adds in from the body

// removes from mesh
void link_free(link_t link);

// load in the key to existing link
link_t link_load(link_t link, uint8_t csid, lob_t key);

// try to turn a path into a pipe and add it to the link
pipe_t link_path(link_t link, lob_t path);

// just add a pipe directly
link_t link_pipe(link_t link, pipe_t pipe);

// iterate through existing pipes for a link
pipe_t link_pipes(link_t link, pipe_t after);

// add a custom outgoing handshake packet for this link
link_t link_handshake(link_t link, lob_t handshake);

// process a decrypted channel packet
link_t link_receive(link_t link, lob_t inner, pipe_t pipe);

// process an incoming handshake
link_t link_receive_handshake(link_t link, lob_t handshake, pipe_t pipe);

// try to deliver this packet to the best pipe
link_t link_send(link_t link, lob_t inner);

// return current handshake(s)
lob_t link_handshakes(link_t link);

// send current handshake(s) to all pipes and return them
lob_t link_sync(link_t link);

// generate new encrypted handshake(s) and sync
lob_t link_resync(link_t link);

// is the other endpoint connected and the link available, NULL if not
link_t link_up(link_t link);

// create/track a new channel for this open
e3x_channel_t link_channel(link_t link, lob_t open);

// set up internal handler for all incoming packets on this channel
link_t link_handle(link_t link, e3x_channel_t c3, void (*handle)(link_t link, e3x_channel_t c3, void *arg), void *arg);

// encrypt and send any outgoing packets for this channel, send the inner if given
link_t link_flush(link_t link, e3x_channel_t c3, lob_t inner);

// encrypt and send this one packet on this pipe
link_t link_direct(link_t link, lob_t inner, pipe_t pipe);


#endif
