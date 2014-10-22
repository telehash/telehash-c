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

// how many network transports can any mesh support
#ifndef MAXNET
#define MAXNET 8
#endif

struct mesh_struct
{
  hashname_t id;
  lob_t keys;
  self3_t self;
//  bucket_t seeds, active;
  lob_t out, last; // packets waiting to be delivered
  lob_t parts;
//  chan_t chans; // channels waiting to be processed
  uint32_t uid, tick;
  int cap, window;
//  uint8_t isSeed;
  xht_t index;
  // network transport handlers to get a pipe for a path
  pipe_t (*net[MAXNET])(link_t link, lob_t path);
//  void (*handler)(struct mesh_struct *, hashname_t); // called w/ a hn that has no key info
};

// pass in a prime for the main index of hashnames+links+channels, 0 to use compiled default
mesh_t mesh_new(uint32_t prime);
mesh_t mesh_free(mesh_t mesh);

// must be called to initialize to a hashname from keys/secrets, return !0 if failed
uint8_t mesh_load(mesh_t mesh, lob_t secrets, lob_t keys);

// creates a new mesh identity, returns secrets
lob_t mesh_generate(mesh_t mesh);

// add a network transport to this mesh to handle future added paths
uint8_t mesh_net(mesh_t mesh, pipe_t (*net)(link_t link, lob_t path));

// processes incoming packet, it will take ownership of p
uint8_t mesh_receive(mesh_t mesh, lob_t p, pipe_t pipe);

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