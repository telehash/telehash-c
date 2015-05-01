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
pipe_t net_serial_add(net_serial_t net, const char *name, int (*read)(void), int (*write)(uint8_t *buf, size_t len), uint8_t buffer);

// manually send a packet down a named pipe (discovery, etc)
net_serial_t net_serial_send(net_serial_t net, const char *name, lob_t packet);

// check all pipes for data
net_serial_t net_serial_loop(net_serial_t net);

#ifdef __cplusplus
}
#endif

#endif
