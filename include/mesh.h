#ifndef mesh_h
#define mesh_h

typedef struct mesh_struct *mesh_t;

#include "e3x.h"
#include "lib.h"
#include "util.h"
#include "pipe.h"
#include "link.h"

struct mesh_struct
{
  hashname_t id;
  char *uri;
  lob_t keys, paths;
  e3x_self_t self;
  xht_t index;
  void *on; // internal list of triggers
  // shared network info
  uint16_t port_local, port_public;
  char *ipv4_local, *ipv4_public;
  lob_t handshakes, cached; // handshakes
};

// pass in a prime for the main index of hashnames+links+channels, 0 to use compiled default
mesh_t mesh_new(uint32_t prime);
mesh_t mesh_free(mesh_t mesh);

// must be called to initialize to a hashname from keys/secrets, return !0 if failed
uint8_t mesh_load(mesh_t mesh, lob_t secrets, lob_t keys);

// creates and loads a new random hashname, returns secrets if it needs to be saved/reused
lob_t mesh_generate(mesh_t mesh);

// return the best current URI to this endpoint, optional base
char *mesh_uri(mesh_t mesh, char *base);

// generate json of mesh keys and current paths
lob_t mesh_json(mesh_t mesh);

// creates a link from the json format of {"hashname":"...","keys":{},"paths":[]}, optional direct pipe too
link_t mesh_add(mesh_t mesh, lob_t json, pipe_t pipe);

// return only if this hashname is currently linked (in any state)
link_t mesh_linked(mesh_t mesh, char *hashname);

// add a custom outgoing handshake packet to all links
mesh_t mesh_handshake(mesh_t mesh, lob_t handshake);

// query the cache of handshakes for a matching one with a specific type
lob_t mesh_handshakes(mesh_t mesh, lob_t handshake, char *type);

// processes incoming packet, it will take ownership of packet
uint8_t mesh_receive(mesh_t mesh, lob_t packet, pipe_t pipe);

// process any unencrypted handshake packet, cache if needed
link_t mesh_receive_handshake(mesh_t mesh, lob_t handshake, pipe_t pipe);

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
void mesh_on_open(mesh_t mesh, char *id, lob_t (*open)(link_t link, lob_t open));
lob_t mesh_open(mesh_t mesh, link_t link, lob_t open);


#endif
