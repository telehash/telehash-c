#ifndef net_udp4_h
#define net_udp4_h

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "mesh.h"

// overall server
typedef struct net_udp4_struct *net_udp4_t;

// create a new listening udp server
net_udp4_t net_udp4_new(mesh_t mesh, lob_t options);
net_udp4_t net_udp4_free(net_udp4_t net);

// send/receive any waiting frames, delivers packets into mesh
net_udp4_t net_udp4_process(net_udp4_t net);

// return server socket handle / port
int net_udp4_socket(net_udp4_t net);
uint16_t net_udp4_port(net_udp4_t net);

// send a packet directly
net_udp4_t net_udp4_direct(net_udp4_t net, lob_t packet, char *ip, uint16_t port);

#endif // POSIX

#endif // net_udp4_h
