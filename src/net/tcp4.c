#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "net_tcp4.h"


// this is just a minimal tcp4 transport backing for testing, it should only serve as an example for using a real socket event lib

// individual pipe local info
typedef struct pipe_tcp4_struct
{
  struct sockaddr_in sa;
  int client;
  net_tcp4_t net;
  util_chunks_t chunks;
} *pipe_tcp4_t;

// just make sure it's connected
pipe_tcp4_t tcp4_to(pipe_t pipe)
{
  struct sockaddr_in addr;
  int ret;
  pipe_tcp4_t to;
  if(!pipe || !pipe->arg) return NULL;
  to = (pipe_tcp4_t)pipe->arg;
  if(to->client > 0) return to;
  
  // no socket yet, connect one
  if((to->client = socket(AF_INET, SOCK_STREAM, 0)) <= 0) return LOG("client socket faied %s, will retry next send",strerror(errno));

  // try to bind to our server port for some reflection
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(to->net->port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(to->client, (struct sockaddr *)&addr, sizeof(struct sockaddr)); // optional, ignore fail

  fcntl(to->client, F_SETFL, O_NONBLOCK);
  ret = connect(to->client, (struct sockaddr *)&(to->sa), sizeof(struct sockaddr));
  if(ret < 0 && errno != EWOULDBLOCK && errno != EINPROGRESS)
  {
    LOG("client socket connect faied to %s: %s, will retry next send",pipe->id,strerror(errno));
    close(to->client);
    to->client = 0;
    return NULL;
  }

  LOG("connected to %s",pipe->id);
  return to;
}

// do all chunk/socket stuff
pipe_t tcp4_flush(pipe_t pipe)
{
  ssize_t len;
  lob_t packet;
  uint8_t buf[256];
  pipe_tcp4_t to = tcp4_to(pipe);
  if(!to) return NULL;

  if(util_chunks_len(to->chunks))
  {
    while((len = write(to->client, util_chunks_write(to->chunks), util_chunks_len(to->chunks))) > 0)
    {
      LOG("wrote %d bytes to %s",len,pipe->id);
      util_chunks_written(to->chunks, (size_t)len);
      LOG("writeat %d written %d",to->chunks->writeat,to->chunks->writing);
    }
  }

  while((len = read(to->client, buf, 256)) > 0)
  {
    LOG("reading %d bytes from %s",len,pipe->id);
    util_chunks_read(to->chunks, buf, (size_t)len);
  }

  // any incoming full packets can be received
  while((packet = util_chunks_receive(to->chunks))) mesh_receive(to->net->mesh, packet, pipe);

  if(len < 0 && errno != EWOULDBLOCK && errno != EINPROGRESS)
  {
    LOG("socket error to %s: %s",pipe->id,strerror(errno));
    close(to->client);
    to->client = 0;
  }

  return pipe;
}

// chunkize a packet
void tcp4_send(pipe_t pipe, lob_t packet, link_t link)
{
  pipe_tcp4_t to = tcp4_to(pipe);
  if(!to || !packet || !link) return;

  util_chunks_send(to->chunks, packet);
  tcp4_flush(pipe);
}

pipe_t tcp4_free(pipe_t pipe)
{
  pipe_tcp4_t to;
  if(!pipe) return NULL;
  to = (pipe_tcp4_t)pipe->arg;
  if(!to) return LOG("internal error, invalid pipe, leaking it");

  xht_set(to->net->pipes,pipe->id,NULL);
  pipe_free(pipe);
  if(to->client > 0) close(to->client);
  util_chunks_free(to->chunks);
  free(to);
  return NULL;
}

// internal, get or create a pipe
pipe_t tcp4_pipe(net_tcp4_t net, char *ip, int port)
{
  pipe_t pipe;
  pipe_tcp4_t to;
  char id[23];

  snprintf(id,23,"%s:%d",ip,port);
  pipe = xht_get(net->pipes,id);
  if(pipe) return pipe;

  // create new tcp4 pipe
  if(!(pipe = pipe_new("tcp4"))) return NULL;
  if(!(pipe->arg = to = malloc(sizeof (struct pipe_tcp4_struct)))) return pipe_free(pipe);
  memset(to,0,sizeof (struct pipe_tcp4_struct));
  to->net = net;
  to->sa.sin_family = AF_INET;
  inet_aton(ip, &(to->sa.sin_addr));
  to->sa.sin_port = htons(port);
  if(!(to->chunks = util_chunks_new(0))) return tcp4_free(pipe);

  // set up pipe
  pipe->id = strdup(id);
  xht_set(net->pipes,pipe->id,pipe);
  pipe->send = tcp4_send;

  return pipe;
}


pipe_t tcp4_path(link_t link, lob_t path)
{
  net_tcp4_t net;
  char *ip;
  int port;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, "net_tcp4"))) return NULL;
  if(util_cmp("tcp4",lob_get(path,"type"))) return NULL;
  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  return tcp4_pipe(net, ip, port);
}

net_tcp4_t net_tcp4_new(mesh_t mesh, lob_t options)
{
  int port, sock, opt = 1;
  unsigned int pipes;
  net_tcp4_t net;
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
  if(bind(sock, (struct sockaddr*)&sa, size) < 0)
  {
    close(sock);
    return LOG("bind failed %s",strerror(errno));
  }
  getsockname(sock, (struct sockaddr*)&sa, &size);
  if(listen(sock, 10) < 0)
  {
    close(sock);
    return LOG("listen failed %s",strerror(errno));
  }
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt , sizeof(int));
  fcntl(sock, F_SETFL, O_NONBLOCK);

  if(!(net = malloc(sizeof (struct net_tcp4_struct))))
  {
    close(sock);
    return LOG("OOM");
  }
  memset(net,0,sizeof (struct net_tcp4_struct));
  net->server = sock;
  net->port = ntohs(sa.sin_port);
  net->pipes = xht_new(pipes);

  // connect us to this mesh
  net->mesh = mesh;
  xht_set(mesh->index, "net_tcp4", net);
  mesh_on_path(mesh, "net_tcp4", tcp4_path);
  
  // convenience
  net->path = lob_new();
  lob_set(net->path,"type","tcp4");
  lob_set(net->path,"ip","127.0.0.1");
  lob_set_int(net->path,"port",net->port);
  mesh->paths = lob_push(mesh->paths, net->path);

  return net;
}

void net_tcp4_free(net_tcp4_t net)
{
  if(!net) return;
  close(net->server);
  xht_free(net->pipes);
//  lob_free(net->path); // managed by mesh->paths
  free(net);
  return;
}

// process any new incoming connections
void net_tcp4_accept(net_tcp4_t net)
{
  struct sockaddr_in addr;
  int client;
  pipe_t pipe;
  pipe_tcp4_t to;
  socklen_t size = sizeof(struct sockaddr_in);

  while((client = accept(net->server, (struct sockaddr *)&addr,&size)) > 0)
  {
    fcntl(client, F_SETFL, O_NONBLOCK);
    if(!(pipe = tcp4_pipe(net, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port))))
    {
      close(client);
      continue;
    }
    LOG("incoming connection from %s",pipe->id);
    to = (pipe_tcp4_t)pipe->arg;
    if(to->client > 0) close(to->client);
    to->client = client;
  }

}

// check a single pipe's socket for any read/write activity
static void _walkflush(xht_t h, const char *key, void *val, void *arg)
{
  tcp4_flush((pipe_t)val);
}

net_tcp4_t net_tcp4_loop(net_tcp4_t net)
{
  net_tcp4_accept(net);
  xht_walk(net->pipes, _walkflush, NULL);
  return net;
}

#endif
