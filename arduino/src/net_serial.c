#include "net_serial.h"


// do all chunk/socket stuff
pipe_t serial_flush(pipe_t pipe)
{
  /*
  ssize_t len;
  lob_t packet;
  uint8_t buf[256];
  pipe_serial_t to = serial_to(pipe);
  if(!to) return NULL;

  if(util_chunks_len(to->chunks))
  {
    while((len = write(to->client, util_chunks_write(to->chunks), util_chunks_len(to->chunks))) > 0)
    {
      LOG("wrote %d bytes to %s",len,pipe->id);
      util_chunks_written(to->chunks, (size_t)len);
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
  */
  return NULL;
}

// chunkize a packet
void serial_send(pipe_t pipe, lob_t packet, link_t link)
{
  /*
  pipe_serial_t to = serial_to(pipe);
  if(!to || !packet || !link) return;

  util_chunks_send(to->chunks, packet);
  serial_flush(pipe);
  */
}

pipe_t serial_free(pipe_t pipe)
{
  /*
  pipe_serial_t to;
  if(!pipe) return NULL;
  to = (pipe_serial_t)pipe->arg;
  if(!to) return LOG("internal error, invalid pipe, leaking it");

  xht_set(to->net->pipes,pipe->id,NULL);
  pipe_free(pipe);
  if(to->client > 0) close(to->client);
  util_chunks_free(to->chunks);
  free(to);
  */
  return NULL;
}

// internal, get or create a pipe
pipe_t serial_pipe(net_serial_t net)
{
  pipe_t pipe = NULL;
  /*
  pipe_serial_t to;
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


pipe_t serial_path(link_t link, lob_t path)
{
  net_serial_t net;
  char *ip;
  int port;

  /*
  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, "net_serial"))) return NULL;
  if(util_cmp("serial",lob_get(path,"type"))) return NULL;
  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  */
  return serial_pipe(net);
}

net_serial_t net_serial_new(mesh_t mesh, int (*read)(void), int (*write)(uint8_t *buf, size_t len))
{
  net_serial_t net = NULL;
  return net;
  /*
  int port, sock, opt = 1;
  unsigned int pipes;
  net_serial_t net;
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

  return net;
  */
}

void net_serial_free(net_serial_t net)
{
  if(!net) return;
//  lob_free(net->path); // managed by mesh->paths
  free(net);
  return;
}

// process any new incoming connections
void net_serial_accept(net_serial_t net)
{
  /*
  struct sockaddr_in addr;
  int client;
  pipe_t pipe;
  pipe_serial_t to;
  socklen_t size = sizeof(struct sockaddr_in);

  while((client = accept(net->server, (struct sockaddr *)&addr,&size)) > 0)
  {
    fcntl(client, F_SETFL, O_NONBLOCK);
    if(!(pipe = serial_pipe(net, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)))) continue;
    LOG("incoming connection from %s",pipe->id);
    to = (pipe_serial_t)pipe->arg;
    if(to->client > 0) close(to->client);
    to->client = client;
  }
*/
}


net_serial_t net_serial_loop(net_serial_t net)
{
  net_serial_accept(net);
  serial_flush(NULL);
  return net;
}
