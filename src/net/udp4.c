#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "net_udp4.h"

// individual pipe local info
typedef struct pipe_udp4_struct
{
  struct sockaddr_in sa;
  net_udp4_t net;
} *pipe_udp4_t;

void udp4_send(pipe_t pipe, lob_t packet, link_t link)
{
  uint8_t *cloaked;
  pipe_udp4_t to = (pipe_udp4_t)pipe->arg;

  if(!to || !packet || !link) return;
  LOG("udp4 to %s",hashname_short(link->id));
  // TODO, determine MTU capacity left and add random # of rounds within that
  cloaked = lob_cloak(packet, 1);
  if(sendto(to->net->server, cloaked, lob_len(packet)+8, 0, (struct sockaddr *)&(to->sa), sizeof(struct sockaddr_in)) < 0) LOG("sendto failed: %s",strerror(errno));
  free(cloaked);
  lob_free(packet);
}

// internal, get or create a pipe
pipe_t udp4_pipe(net_udp4_t net, char *ip, int port)
{
  pipe_t pipe;
  pipe_udp4_t to;
  char id[23];

  snprintf(id,23,"%s:%d",ip,port);
  pipe = xht_get(net->pipes,id);
  if(pipe) return pipe;

  LOG("new pipe to %s",id);

  // create new udp4 pipe
  if(!(to = malloc(sizeof (struct pipe_udp4_struct)))) return LOG("OOM");
  memset(to,0,sizeof (struct pipe_udp4_struct));
  to->net = net;
  to->sa.sin_family = AF_INET;
  inet_aton(ip, &(to->sa.sin_addr));
  to->sa.sin_port = htons(port);

  // create the general pipe object to return and save reference to it
  if(!(pipe = pipe_new("udp4")))
  {
    free(to);
    return LOG("OOM");
  }
  pipe->id = strdup(id);
  xht_set(net->pipes,pipe->id,pipe);

  pipe->arg = to;
  pipe->send = udp4_send;

  return pipe;
}

pipe_t udp4_path(link_t link, lob_t path)
{
  net_udp4_t net;
  char *ip;
  int port;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, "net_udp4"))) return NULL;
  if(util_cmp("udp4",lob_get(path,"type"))) return NULL;
  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  
  return udp4_pipe(net, ip, port);
}

net_udp4_t net_udp4_new(mesh_t mesh, lob_t options)
{
  int port, sock;
  unsigned int pipes;
  net_udp4_t net;
  struct sockaddr_in sa;
  socklen_t size = sizeof(struct sockaddr_in);
  
  port = lob_get_int(options,"port");
  if(!port) port = mesh->port_local; // might be another in use
  pipes = lob_get_uint(options,"pipes");
  if(!pipes) pipes = 11; // hashtable for active pipes

  // create a udp socket
  if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 ) return LOG("failed to create socket %s",strerror(errno));

  memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (sock, (struct sockaddr*)&sa, size) < 0)
  {
    close(sock);
    return LOG("bind failed %s",strerror(errno));
  }
  getsockname(sock, (struct sockaddr*)&sa, &size);

  if(!(net = malloc(sizeof (struct net_udp4_struct))))
  {
    close(sock);
    return LOG("OOM");
  }
  memset(net,0,sizeof (struct net_udp4_struct));
  net->server = sock;
  net->port = ntohs(sa.sin_port);
  if(!mesh->port_local) mesh->port_local = (uint16_t)net->port; // use ours as the default if no others
  net->pipes = xht_new(pipes);

  // connect us to this mesh
  net->mesh = mesh;
  xht_set(mesh->index, "net_udp4", net);
  mesh_on_path(mesh, "net_udp4", udp4_path);
  
  // convenience
  net->path = lob_new();
  lob_set(net->path,"type","udp4");
  lob_set(net->path,"ip","127.0.0.1");
  lob_set_int(net->path,"port",net->port);
  mesh->paths = lob_push(mesh->paths, net->path);

  return net;
}

void net_udp4_free(net_udp4_t net)
{
  if(!net) return;
  close(net->server);
  xht_free(net->pipes);
//  lob_free(net->path); owned by mesh
  free(net);
  return;
}

net_udp4_t net_udp4_receive(net_udp4_t net)
{
  unsigned char buf[2048];
  struct sockaddr_in sa;
  ssize_t len;
  size_t salen;
  lob_t packet;
  pipe_t pipe;
  
  if(!net) return LOG("bad args");

  salen = sizeof(sa);
  memset(&sa,0,salen);
  len = recvfrom(net->server, buf, sizeof(buf), 0, (struct sockaddr *)&sa, (socklen_t *)&salen);

  if(len < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return net;
  if(len <= 0) return LOG("recvfrom error %s",strerror(errno));

  packet = lob_decloak(buf, (size_t)len);
  if(!packet)
  {
    LOG("parse error from %s on %d bytes",inet_ntoa(sa.sin_addr),len);
    return net;
  }

  // create the id and look for existing pipe
  pipe = udp4_pipe(net, inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
  mesh_receive(net->mesh, packet, pipe);
  
  return net;
}

#endif
