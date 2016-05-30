#ifndef mesh_h
#define mesh_h

typedef struct mesh_struct *mesh_t;
typedef struct link_struct *link_t;
typedef struct chan_struct *chan_t;



#include "e3x.h"
#include "lib.h"
#include "util.h"
#include "chan.h"
#include "link.h"

struct mesh_struct
{
  hashname_t id;
  lob_t keys, paths;
  e3x_self_t self;
  void *on; // internal list of triggers
  // shared network info
  uint16_t port_local, port_public;
  char *ipv4_local, *ipv4_public;
  link_t links;
};

// pass in a prime for the main index of hashnames+links+channels, 0 to use compiled default
mesh_t mesh_new(uint32_t prime);
mesh_t mesh_free(mesh_t mesh);

// must be called to initialize to a hashname from keys/secrets, return !0 if failed
uint8_t mesh_load(mesh_t mesh, lob_t secrets, lob_t keys);

// creates and loads a new random hashname, returns secrets if it needs to be saved/reused
lob_t mesh_generate(mesh_t mesh);

// simple accessors
hashname_t mesh_id(mesh_t mesh);
lob_t mesh_keys(mesh_t mesh);

// generate json of mesh keys and current paths
lob_t mesh_json(mesh_t mesh);

// generate json for all links, returns lob list
lob_t mesh_links(mesh_t mesh);

// creates a link from the json format of {"hashname":"...","keys":{},"paths":[]}
link_t mesh_add(mesh_t mesh, lob_t json);

// return only if this hashname (full or short) is currently linked (in any state)
link_t mesh_linked(mesh_t mesh, char *hn, size_t len);
link_t mesh_linkid(mesh_t mesh, hashname_t id); // TODO, clean this up

// remove this link, will event it down and clean up during next process()
mesh_t mesh_unlink(link_t link);

// processes incoming packet, it will take ownership of packet, returns link delivered to if success
link_t mesh_receive(mesh_t mesh, lob_t packet);

// process any unencrypted handshake packet
link_t mesh_receive_handshake(mesh_t mesh, lob_t handshake);

// process any channel timeouts based on the current/given time
mesh_t mesh_process(mesh_t mesh, uint32_t now);

// callback when the mesh is free'd
void mesh_on_free(mesh_t mesh, char *id, void (*free)(mesh_t mesh));

// callback when a path needs to be turned into a pipe
void mesh_on_path(mesh_t mesh, char *id, link_t (*path)(link_t link, lob_t path));
link_t mesh_path(mesh_t mesh, link_t link, lob_t path);

// callback when an unknown hashname is discovered
void mesh_on_discover(mesh_t mesh, char *id, link_t (*discover)(mesh_t mesh, lob_t discovered));
void mesh_discover(mesh_t mesh, lob_t discovered);

// callback when a link changes state created/up/down
void mesh_on_link(mesh_t mesh, char *id, void (*link)(link_t link));
void mesh_link(mesh_t mesh, link_t link);

// callback when a new incoming channel is requested
void mesh_on_open(mesh_t mesh, char *id, lob_t (*open)(link_t link, lob_t open));
lob_t mesh_open(mesh_t mesh, link_t link, lob_t open);


#endif
