#ifndef ext_block_h
#define ext_block_h

#include "mesh.h"

// add block channel support
mesh_t ext_block(mesh_t mesh);

// get the next incoming block, if any, packet->arg is the link it came from
lob_t ext_block_receive(mesh_t mesh);

// creates/reuses a single default block channel on the link
link_t ext_block_send(link_t link, lob_t block);

// TODO, handle multiple block channels per link, and custom packets on open

#endif
