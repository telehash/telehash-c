#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "net_udp4.h"

// individual pipe local info
typedef struct pipe_struct
{
  link_t link;
  util_frames_t frames;
  net_udp4_t net;
  struct pipe_struct *next;
  struct sockaddr_in sa;
} *pipe_t;

// overall server
struct net_udp4_struct
{
  mesh_t mesh;
  pipe_t pipes;
  int server;
  int port;
};

static pipe_t pipe_free(pipe_t pipe)
{
  if(!pipe || !pipe->net || !pipe->net->pipes) return LOG("bad args");
  LOG_DEBUG("dropping pipe %s:%u",inet_ntoa(pipe->sa.sin_addr), ntohs(pipe->sa.sin_port));

  // remove from pipes list
  if(pipe == pipe->net->pipes) pipe->net->pipes = pipe->next;
  else {
    pipe_t p = pipe->net->pipes;
    while(p && p->next != pipe) p = p->next;
    if(!p) LOG_WARN("pipe not found for %s:%u",inet_ntoa(pipe->sa.sin_addr), ntohs(pipe->sa.sin_port));
    else p->next = pipe->next;
  }

  pipe->frames = util_frames_free(pipe->frames);
  free(pipe);
  return NULL;
}


link_t udp4_send(link_t link, lob_t packet, void *arg)
{
  pipe_t pipe = (pipe_t)arg;
  if(!pipe || !link) return NULL;
  
  // request to drop;
  if(!packet)
  {
    pipe = pipe_free(pipe);
    return link;
  }

  LOG_CRAZY("send to %s at %s:%u",hashname_short(link->id),inet_ntoa(pipe->sa.sin_addr), ntohs(pipe->sa.sin_port));
  util_frames_send(pipe->frames,packet);

  return link;
}

// internal, get or create a pipe
pipe_t udp4_pipe(net_udp4_t net, struct sockaddr_in *from)
{
  pipe_t to;

  // find existing
  for(to = net->pipes; to; to = to->next) if(memcmp(&(to->sa.sin_addr),&(from->sin_addr),sizeof(struct in_addr)) == 0 && to->sa.sin_port == from->sin_port) return to;

  LOG("new pipe to %s:%u",inet_ntoa(from->sin_addr), ntohs(from->sin_port));

  // create new udp4 pipe
  if(!(to = malloc(sizeof (struct pipe_struct)))) return LOG("OOM");
  memset(to,0,sizeof (struct pipe_struct));
  to->net = net;
  to->sa.sin_family = AF_INET;
  to->sa.sin_addr = from->sin_addr;
  to->sa.sin_port = from->sin_port;
  to->frames = util_frames_new(252);
  
  // link into list
  to->next = net->pipes;
  net->pipes = to;

  return to;
}

net_udp4_t net_udp4_new(mesh_t mesh, lob_t options)
{
  int port, sock;
  net_udp4_t net;
  struct sockaddr_in sa;
  socklen_t size = sizeof(struct sockaddr_in);
  
  port = lob_get_int(options,"port");
  if(!port) port = mesh->port_local; // might be another in use

  // create a udp socket
  if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 ) return LOG_ERROR("failed to create socket %s",strerror(errno));

  memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (sock, (struct sockaddr*)&sa, size) < 0)
  {
    close(sock);
    return LOG_ERROR("bind failed %s",strerror(errno));
  }
  getsockname(sock, (struct sockaddr*)&sa, &size);

  if(!(net = malloc(sizeof (struct net_udp4_struct))))
  {
    close(sock);
    return LOG_ERROR("OOM");
  }
  memset(net,0,sizeof (struct net_udp4_struct));
  net->mesh = mesh;
  net->server = sock;
  net->port = ntohs(sa.sin_port);
  if(!mesh->port_local) mesh->port_local = (uint16_t)net->port; // use ours as the default if no others

  return net;
}

net_udp4_t net_udp4_free(net_udp4_t net)
{
  if(!net) return NULL;
  LOG_DEBUG("closing udp4 transport on %u",net->port);
  close(net->server);
  free(net);
  return NULL;
}

net_udp4_t net_udp4_process(net_udp4_t net)
{
  if(!net) return LOG_WARN("bad args");

  struct sockaddr_in sa;
  size_t salen = sizeof(sa);
  memset(&sa,0,salen);
  uint8_t frame[256];
  
  // try receiving anything waiting
  pipe_t pipe = NULL;
  while(1)
  {
    ssize_t len = recvfrom(net->server, frame, sizeof(frame), 0, (struct sockaddr *)&sa, (socklen_t *)&salen);
    if(len < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    if(len <= 0) return LOG_WARN("recvfrom error %s",strerror(errno));
    
    // get the pipe and return
    if(!pipe || memcmp(&(pipe->sa.sin_addr), &(sa.sin_addr), sizeof(struct in_addr)) || pipe->sa.sin_port != sa.sin_port) pipe = udp4_pipe(net, &sa);
    if(pipe)
    {
      LOG_CRAZY("receive from %s at %s:%u",(pipe->link)?hashname_short(pipe->link->id):"unknown",inet_ntoa(pipe->sa.sin_addr), ntohs(pipe->sa.sin_port));
      util_frames_inbox(pipe->frames, frame, NULL);
    }
  }

  // process each pipe also
  pipe_t next = NULL;
  for(pipe = net->pipes;pipe;pipe = next)
  {
    next = pipe->next;
    
    // process received full packets
    lob_t packet = NULL;
    while((packet = util_frames_receive(pipe->frames)))
    {
      link_t link = mesh_receive(net->mesh, packet);
      if(!link) continue;
      if(link != pipe->link)
      {
        pipe->link = link;
        link_pipe(link,udp4_send,pipe);
      }
    }
    
    // send all/any waiting frames
    while(util_frames_outbox(pipe->frames,frame,NULL))
    {
      if(sendto(net->server, frame, sizeof(frame), 0, (struct sockaddr *)&(pipe->sa), sizeof(struct sockaddr_in)) < 0)
      {
        LOG_WARN("sendto failed: %s to %s:%u",strerror(errno),inet_ntoa(pipe->sa.sin_addr), ntohs(pipe->sa.sin_port));
        break;
      }
      // only continue if sent says there's more
      if(!util_frames_sent(pipe->frames)) break;
    }
  }
  
  return net;
}

int net_udp4_socket(net_udp4_t net)
{
  if(!net) return -1;
  return net->server;
}

#endif // POSIX
