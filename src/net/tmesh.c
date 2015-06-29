#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tmesh.h"


// individual pipe local info
typedef struct pipe_tmesh_struct
{
  net_tmesh_t net;
  link_t link;
  util_chunks_t chunks;
  epoch_t tx, rx;
} *pipe_tmesh_t;

// just make sure it's connected
pipe_tmesh_t tmesh_to(pipe_t pipe)
{
  pipe_tmesh_t to;
  if(!pipe || !pipe->arg) return NULL;
  to = (pipe_tmesh_t)pipe->arg;
  return to;
}

// do hard scheduling stuff
pipe_t tmesh_flush(pipe_t pipe)
{
  pipe_tmesh_t to = tmesh_to(pipe);
  if(!to) return NULL;

  if(util_chunks_len(to->chunks))
  {
    /*
    while((len = write(to->client, util_chunks_write(to->chunks), util_chunks_len(to->chunks))) > 0)
    {
      LOG("wrote %d bytes to %s",len,pipe->id);
      util_chunks_written(to->chunks, (size_t)len);
    }
    */
  }

  /*
  while((len = read(to->client, buf, 256)) > 0)
  {
    LOG("reading %d bytes from %s",len,pipe->id);
    util_chunks_read(to->chunks, buf, (size_t)len);
  }
  */

  // any incoming full packets can be received
//  while((packet = util_chunks_receive(to->chunks))) mesh_receive(to->net->mesh, packet, pipe);

  return pipe;
}

// chunkize a packet
void tmesh_send(pipe_t pipe, lob_t packet, link_t link)
{
  pipe_tmesh_t to = tmesh_to(pipe);
  if(!to || !packet || !link) return;

  util_chunks_send(to->chunks, packet);
  tmesh_flush(pipe);
}

pipe_t tmesh_free(pipe_t pipe)
{
  pipe_tmesh_t to;
  if(!pipe) return NULL;
  to = (pipe_tmesh_t)pipe->arg;
  if(!to) return LOG("internal error, invalid pipe, leaking it");

  xht_set(to->net->pipes,pipe->id,NULL);
  pipe_free(pipe);
  util_chunks_free(to->chunks);
  free(to);
  return NULL;
}

// internal, get or create a pipe
pipe_t tmesh_pipe(net_tmesh_t net, link_t link)
{
  pipe_t pipe;
  pipe_tmesh_t to;
  if(!net || !link) return LOG("bad args");

  pipe = xht_get(net->pipes,link->id->hashname);
  if(pipe) return pipe;

  // create new tmesh pipe
  if(!(pipe = pipe_new("tmesh"))) return NULL;
  if(!(pipe->arg = to = malloc(sizeof (struct pipe_tmesh_struct)))) return pipe_free(pipe);
  memset(to,0,sizeof (struct pipe_tmesh_struct));
  to->net = net;
  to->link = link;
  if(!(to->chunks = util_chunks_new(0))) return tmesh_free(pipe);
  
  // set up pipe
  pipe->id = strdup(link->id->hashname);
  xht_set(net->pipes,pipe->id,pipe);
  pipe->send = tmesh_send;

  return pipe;
}


pipe_t tmesh_path(link_t link, lob_t path)
{
  net_tmesh_t net;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, "net_tmesh"))) return NULL;
  if(util_cmp("tmesh",lob_get(path,"type"))) return NULL;
//  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
//  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  return tmesh_pipe(net, link);
}

net_tmesh_t net_tmesh_new(mesh_t mesh, lob_t options)
{
  unsigned int pipes;
  net_tmesh_t net;
  
  pipes = lob_get_uint(options,"pipes");
  if(!pipes) pipes = 11; // hashtable for active pipes

  if(!(net = malloc(sizeof (struct net_tmesh_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_tmesh_struct));
  net->pipes = xht_new(pipes);

  // connect us to this mesh
  net->mesh = mesh;
  xht_set(mesh->index, "net_tmesh", net);
  mesh_on_path(mesh, "net_tmesh", tmesh_path);
  
  // convenience
  net->path = lob_new();
  lob_set(net->path,"type","tmesh");
  mesh->paths = lob_push(mesh->paths, net->path);

  return net;
}

void net_tmesh_free(net_tmesh_t net)
{
  if(!net) return;
  xht_free(net->pipes);
//  lob_free(net->path); // managed by mesh->paths
  free(net);
  return;
}

// process any new incoming connections
void net_tmesh_accept(net_tmesh_t net)
{

  /*
  while((client = accept(net->server, (struct sockaddr *)&addr,&size)) > 0)
  {
    fcntl(client, F_SETFL, O_NONBLOCK);
    if(!(pipe = tmesh_pipe(net, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)))) continue;
    LOG("incoming connection from %s",pipe->id);
    to = (pipe_tmesh_t)pipe->arg;
    if(to->client > 0) close(to->client);
    to->client = client;
  }
  */

}

// check a single pipe's socket for any read/write activity
static void _walkflush(xht_t h, const char *key, void *val, void *arg)
{
  tmesh_flush((pipe_t)val);
}

net_tmesh_t net_tmesh_loop(net_tmesh_t net)
{
  net_tmesh_accept(net);
  xht_walk(net->pipes, _walkflush, NULL);
  return net;
}

