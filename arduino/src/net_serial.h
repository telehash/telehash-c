#ifndef serial_h
#define serial_h

#include <stdio.h>
#include <stdlib.h>
#include <telehash.h>

// a serial pipe
typedef struct net_serial_struct
{
  int (*write)(uint8_t *buf, size_t len);
  int (*read)(void);
  mesh_t mesh;
  util_chunks_t chunks;
  lob_t path;
} *net_serial_t;

// create a new serial handler
net_serial_t net_serial_new(mesh_t mesh, int (*read)(void), int (*write)(uint8_t *buf, size_t len));
void net_serial_free(net_serial_t net);

// process incoming data from the read
net_serial_t net_serial_loop(net_serial_t net);


#endif
