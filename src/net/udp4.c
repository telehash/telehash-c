#include <errno.h>
#include <string.h>
#include "udp4.h"

#define MID "net_udp4"

// individual pipe local info
typedef struct pipe_udp4_struct
{
  struct sockaddr_in sa;
  net_udp4_t net;
} *pipe_udp4_t;

void udp4_send(pipe_t pipe, lob_t packet, link_t link)
{
  pipe_udp4_t to = (pipe_udp4_t)pipe->arg;

  if(!to || !packet || !link) return;
  LOG("udp4 to %s",link->id->hashname);

  if(sendto(to->net->server, lob_raw(packet), lob_len(packet), 0, (struct sockaddr *)&(to->sa), sizeof(struct sockaddr_in)) < 0) LOG("sendto failed: %s",strerror(errno));
}

pipe_t udp4_path(link_t link, lob_t path)
{
  net_udp4_t net;
  pipe_t pipe;
  pipe_udp4_t to;
  char *ip;
  int port;
  char id[23];

  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, MID))) return NULL;
  if(util_cmp("udp4",lob_get(path,"type"))) return NULL;
  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  
  // create the id and look for existing pipe
  snprintf(id,23,"%s:%d",ip,port);
  pipe = xht_get(net->pipes,id);
  if(pipe) return pipe;
  
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
  pipe->arg = to;
  xht_set(net->pipes,pipe->id,pipe);

  return pipe;
}

net_udp4_t net_udp4_new(mesh_t mesh, int port)
{
  net_udp4_t net;

  if(!(net = malloc(sizeof (struct net_udp4_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_udp4_struct));
  
  // TODO server
  net->port = port;
  net->mesh = mesh;

  /*
  int sock;
  struct	sockaddr_in sad;
  struct timeval tv;

  // create a udp socket
  if( (sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 )
  {
    printf("failed to create socket\n");
    return -1;
  }
  memset(&sad,0,sizeof(sad));
  sad.sin_family = AF_INET;
  sad.sin_port = htons(port);
  sad.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (sock, (struct sockaddr *)&sad, sizeof(sad)) < 0)
  {
    perror("bind failed");
    return -1;
  }
  tv.tv_sec = ms/1000;
  tv.tv_usec = (ms%1000)*1000;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
  {
    perror("setsockopt");
    return -1;
  }
  */

  return net;
}

void net_udp4_free(net_udp4_t net)
{
  free(net);
  return;
}

uint8_t net_udp4_receive(net_udp4_t net)
{
  return 1;
  /*
  unsigned char buf[2048];
  struct sockaddr_in sa;
  int len, salen;
  lob_t p;
  
  salen = sizeof(sa);
  memset(&sa,0,salen);
  len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&sa, (socklen_t *)&salen);

  if(len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
  if(len <= 0) return 0;

  sa2path(&sa,in); // inits ip/port from sa
  strcpy(p->ip,inet_ntoa(sa->sin_addr));
  p->port = ntohs(sa->sin_port);
  p = lob_parse(buf,len);
  DEBUG_PRINTF(">>> %s packet %d %s", p->json_len?"open":"line", len, path_json(in));
  switch_receive(s,p,in);
  return 0;
  */
}
