#ifndef mesh_h
#define mesh_h

typedef struct mesh_struct *mesh_t;

#include "e3x.h"
#include "hashname.h"
#include "lob.h"
#include "xht.h"
#include "pipe.h"
#include "link.h"
#include "links.h"
#include "util.h"
#include "platform.h"

struct mesh_struct
{
  hashname_t id;
  lob_t keys;
  self3_t self;
  xht_t index;
  void *on; // internal list of triggers
};

// pass in a prime for the main index of hashnames+links+channels, 0 to use compiled default
mesh_t mesh_new(uint32_t prime);
mesh_t mesh_free(mesh_t mesh);

// must be called to initialize to a hashname from keys/secrets, return !0 if failed
uint8_t mesh_load(mesh_t mesh, lob_t secrets, lob_t keys);

// creates a new mesh identity, returns secrets
lob_t mesh_generate(mesh_t mesh);

// creates a link from the json format of {"hashname":"...","keys":{},"paths":[]}, optional direct pipe too
link_t mesh_add(mesh_t mesh, lob_t json, pipe_t pipe);

// processes incoming packet, it will take ownership of packet
uint8_t mesh_receive(mesh_t mesh, lob_t packet, pipe_t pipe);

// callback when the mesh is free'd
void mesh_on_free(mesh_t mesh, char *id, void (*free)(mesh_t mesh));

// callback when a path needs to be turned into a pipe
void mesh_on_path(mesh_t mesh, char *id, pipe_t (*path)(link_t link, lob_t path));
pipe_t mesh_path(mesh_t mesh, link_t link, lob_t path);

// callback when an unknown hashname is discovered
void mesh_on_discover(mesh_t mesh, char *id, link_t (*discover)(mesh_t mesh, lob_t discovered, pipe_t pipe));
void mesh_discover(mesh_t mesh, lob_t discovered, pipe_t pipe);

// callback when a link changes state created/up/down
void mesh_on_link(mesh_t mesh, char *id, void (*link)(link_t link));
void mesh_link(mesh_t mesh, link_t link);

// callback when a new incoming channel is requested
void mesh_on_open(mesh_t mesh, char *id, void (*open)(link_t link, lob_t open));
void mesh_open(mesh_t mesh, link_t link, lob_t open);

// for any link, validate a request packet
void mesh_on_validate(mesh_t mesh, char *id, lob_t (*validate)(link_t link, lob_t req));

/*

// add hashname as a seed, will automatically trigger a query to it
void mesh_seed(mesh_t s, hashname_t hn);

// this is already called implicitly by mesh_sending, handles timers
void mesh_loop(mesh_t s);

// generate a broadcast/handshake ping packet, only send these locally
lob_t mesh_ping(mesh_t s);

// get a packet to be sent, NULL if none
lob_t mesh_sending(mesh_t s);

// get a channel that has packets to be processed, NULL if none
chan_t mesh_pop(mesh_t s);

// encrypts a packet and adds it to the sending queue, mostly internal use
void mesh_send(mesh_t s, lob_t p);
// adds to sending queue, internal only
void mesh_sendingQ(mesh_t s, lob_t p);

// adjust/set the cap of how many lines to maintain (256) and reliable window packet buffer size (32)
void mesh_capwin(mesh_t s, int cap, int window);

// processes incoming packet, it will free p, leaves in alone (is reusable)
void mesh_receive(mesh_t s, lob_t p, path_t in);

// tries to send an open (if we need to), mostly internal
void mesh_open(mesh_t s, hashname_t to, path_t direct);

// sends a note packet to it's channel if it can, !0 for error
int mesh_note(mesh_t s, lob_t note);
*/

#endif