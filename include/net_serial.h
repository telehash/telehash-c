#ifndef net_serial_h
#define net_serial_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#include "mesh.h"

// overall server
typedef struct net_serial_struct
{
  mesh_t mesh;
  xht_t pipes;
} *net_serial_t;

// create a new serial hub
net_serial_t net_serial_new(mesh_t mesh, lob_t options);
void net_serial_free(net_serial_t net);

// add a named serial pipe and I/O callbacks for it
net_serial_t net_serial_add(mesh_t mesh, const char *name, int (*read)(void), int (*write)(uint8_t *buf, size_t len));

// check all pipes for data
net_serial_t net_serial_loop(net_serial_t net);

#ifdef __cplusplus
}
#endif

#endif
