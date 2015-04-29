#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_serial.h"


// individual pipe local info
typedef struct pipe_serial_struct
{
  int (*read)(void);
  int (*write)(uint8_t *buf, size_t len);
  net_serial_t net;
  util_chunks_t chunks;
} *pipe_serial_t;

// do all chunk/socket stuff
pipe_t serial_flush(pipe_t pipe)
{
  size_t len;
  lob_t packet;
  uint8_t buf[256];
  pipe_serial_t to;
  if(!pipe || !(to = (pipe_serial_t)pipe->arg)) return NULL;

  if(util_chunks_len(to->chunks))
  {
    while((len = to->write(util_chunks_write(to->chunks), util_chunks_len(to->chunks))) > 0)
    {
      LOG("wrote %d bytes to %s",len,pipe->id);
      util_chunks_written(to->chunks, len);
    }
  }

  while((len = to->read()) > 0)
  {
    LOG("reading %d bytes from %s",len,pipe->id);
    util_chunks_read(to->chunks, buf, len);
  }

  // any incoming full packets can be received
  while((packet = util_chunks_receive(to->chunks))) mesh_receive(to->net->mesh, packet, pipe);

  return pipe;
}

// chunkize a packet
void serial_send(pipe_t pipe, lob_t packet, link_t link)
{
  pipe_serial_t to;
  if(!pipe || !packet || !link || !(to = (pipe_serial_t)pipe->arg)) return;

  util_chunks_send(to->chunks, packet);
  serial_flush(pipe);
}


pipe_t serial_path(link_t link, lob_t path)
{
  net_serial_t net;
  pipe_t pipe = NULL;
  pipe_serial_t to;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, "net_serial"))) return NULL;
  if(util_cmp("serial",lob_get(path,"type"))) return NULL;

  /*
  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  
  char id[23];

  snprintf(id,23,"%s:%d",ip,port);
  pipe = xht_get(net->pipes,id);
  if(pipe) return pipe;

  // create new serial pipe
  if(!(pipe = pipe_new("serial"))) return NULL;
  if(!(pipe->arg = to = malloc(sizeof (struct pipe_serial_struct)))) return pipe_free(pipe);
  memset(to,0,sizeof (struct pipe_serial_struct));
  to->net = net;
  to->sa.sin_family = AF_INET;
  inet_aton(ip, &(to->sa.sin_addr));
  to->sa.sin_port = htons(port);
  if(!(to->chunks = util_chunks_new(0))) return serial_free(pipe);
  util_chunks_cloak(to->chunks); // enable cloaking by default

  // set up pipe
  pipe->id = strdup(id);
  xht_set(net->pipes,pipe->id,pipe);
  pipe->send = serial_send;
  */
  return pipe;
}

net_serial_t net_serial_new(mesh_t mesh, lob_t options)
{
  net_serial_t net = NULL;
  /*
  int port, sock, opt = 1;
  unsigned int pipes;
  struct sockaddr_in sa;
  socklen_t size = sizeof(struct sockaddr_in);
  
  port = lob_get_int(options,"port");
  pipes = lob_get_uint(options,"pipes");
  if(!pipes) pipes = 11; // hashtable for active pipes

  // create a udp socket
  if((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) ) < 0 ) return LOG("failed to create socket %s",strerror(errno));

  memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  if(bind(sock, (struct sockaddr*)&sa, size) < 0) return LOG("bind failed %s",strerror(errno));
  getsockname(sock, (struct sockaddr*)&sa, &size);
  if(listen(sock, 10) < 0) return LOG("listen failed %s",strerror(errno));
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt , sizeof(int));
  fcntl(sock, F_SETFL, O_NONBLOCK);

  if(!(net = malloc(sizeof (struct net_serial_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_serial_struct));
  net->server = sock;
  net->port = ntohs(sa.sin_port);
  net->pipes = xht_new(pipes);

  // connect us to this mesh
  net->mesh = mesh;
  xht_set(mesh->index, "net_serial", net);
  mesh_on_path(mesh, "net_serial", serial_path);
  
  // convenience
  net->path = lob_new();
  lob_set(net->path,"type","serial");
  lob_set(net->path,"ip","127.0.0.1");
  lob_set_int(net->path,"port",net->port);
  mesh->paths = lob_push(mesh->paths, net->path);
  */
  return net;
}

void net_serial_free(net_serial_t net)
{
  if(!net) return;
  xht_free(net->pipes);
  free(net);
  return;
}


// check a single pipe's socket for any read/write activity
static void _walkflush(xht_t h, const char *key, void *val, void *arg)
{
  serial_flush((pipe_t)val);
}

net_serial_t net_serial_loop(net_serial_t net)
{
  xht_walk(net->pipes, _walkflush, NULL);
  return net;
}
