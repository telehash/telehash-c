#ifndef util_unix_h
#define util_unix_h

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include "mesh.h"

// load a json file into packet
lob_t util_fjson(char *file);

// load an array of hashnames from a file and add them as links
mesh_t util_links(mesh_t mesh, char *file);

// simple sockets simpler
int util_sock_timeout(int sock, uint32_t ms); // blocking timeout

#endif

#endif
