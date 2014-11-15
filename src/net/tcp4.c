#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "tcp4.h"

// this is just a minimal tcp4 transport backing for testing, it should only serve as an example for using a real socket event lib

// our unique id per mesh
#define MUID "net_tcp4"

// individual pipe local info
typedef struct pipe_tcp4_struct
{
  struct sockaddr_in sa;
  net_tcp4_t net;
} *pipe_tcp4_t;

void tcp4_send(pipe_t pipe, lob_t packet, link_t link)
{
  pipe_tcp4_t to = (pipe_tcp4_t)pipe->arg;

  if(!to || !packet || !link) return;
  LOG("tcp4 to %s",link->id->hashname);

  // TODO chunks
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

  LOG("new pipe to %s",id);

  // create new tcp4 pipe
  if(!(to = malloc(sizeof (struct pipe_tcp4_struct)))) return LOG("OOM");
  memset(to,0,sizeof (struct pipe_tcp4_struct));
  to->net = net;
  to->sa.sin_family = AF_INET;
  inet_aton(ip, &(to->sa.sin_addr));
  to->sa.sin_port = htons(port);

  // create the general pipe object to return and save reference to it
  if(!(pipe = pipe_new("tcp4")))
  {
    free(to);
    return LOG("OOM");
  }
  pipe->id = strdup(id);
  xht_set(net->pipes,pipe->id,pipe);

  pipe->arg = to;
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
  if(!(net = xht_get(link->mesh->index, MUID))) return NULL;
  if(util_cmp("tcp4",lob_get(path,"type"))) return NULL;
  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  
  return tcp4_pipe(net, ip, port);
}

net_tcp4_t net_tcp4_new(mesh_t mesh, lob_t options)
{
  int port, sock, pipes, opt = 1;
  net_tcp4_t net;
  struct sockaddr_in sa;
  socklen_t size = sizeof(struct sockaddr_in);
  
  port = lob_get_int(options,"port");
  pipes = lob_get_int(options,"pipes");
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

  if(!(net = malloc(sizeof (struct net_tcp4_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_tcp4_struct));
  net->server = sock;
  net->port = ntohs(sa.sin_port);
  net->pipes = xht_new(pipes);

  // connect us to this mesh
  net->mesh = mesh;
  xht_set(mesh->index, MUID, net);
  mesh_on_path(mesh, MUID, tcp4_path);
  
  // convenience
  net->path = lob_new();
  lob_set(net->path,"type","tcp4");
  lob_set(net->path,"ip","127.0.0.1");
  lob_set_int(net->path,"port",net->port);

  return net;
}

void net_tcp4_free(net_tcp4_t net)
{
  if(!net) return;
  close(net->server);
  xht_free(net->pipes);
  lob_free(net->path);
  free(net);
  return;
}

net_tcp4_t net_tcp4_loop(net_tcp4_t net)
{
  // TODO accept, bind to our port, nonblock, get pipe
  // TODO loop pipes to read/write chunks
  return net;
}
