#ifndef ext_peer_h
#define ext_peer_h

#include "ext.h"

// enables peer/connect handling for this mesh
mesh_t peer_enable(mesh_t mesh);

// become a router
mesh_t peer_route(mesh_t mesh);

// set this link as the default router for any peer
link_t peer_router(link_t router);

// try to connect this peer via this router (sends an ad-hoc peer request)
link_t peer_connect(link_t peer, link_t router);


#endif

