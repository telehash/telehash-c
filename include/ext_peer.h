#ifndef ext_peer_h
#define ext_peer_h

#include "ext.h"

// try to connect this peer via this router (sends a peer request)
link_t peer_router(link_t peer, link_t router);

// handle incoming peer request to route
lob_t peer_on_open(link_t link, lob_t open);

// turn a peer path into a pipe
pipe_t peer_path(link_t link, lob_t path);

#endif
