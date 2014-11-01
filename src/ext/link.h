#ifndef ext_link_h
#define ext_link_h

#include "mesh.h"

mesh_t ext_link(mesh_t mesh);

// get/change the link status (err to mark down, any status to mark up)
lob_t ext_link_status(link_t link, lob_t status);


#endif
