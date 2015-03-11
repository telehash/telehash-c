#ifndef ext_path_h
#define ext_path_h

#include "ext.h"

// send a path ping and get callback event
link_t path_ping(link_t link, void (*pong)(link_t link, lob_t status, void *arg), void *arg);

lob_t path_on_open(link_t link, lob_t open);

#endif
