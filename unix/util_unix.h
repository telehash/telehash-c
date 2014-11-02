#ifndef util_unix_h
#define util_unix_h

#include "mesh.h"

// load a json file into packet
lob_t util_fjson(char *file);

// load an array of hashnames from a file and add them as links
mesh_t util_links(mesh_t mesh, char *file);

#endif