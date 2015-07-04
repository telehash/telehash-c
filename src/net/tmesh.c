#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tmesh.h"


// just make sure it's connected
mote_t tmesh_to(pipe_t pipe)
{
  mote_t to;
  if(!pipe || !pipe->arg) return NULL;
  to = (mote_t)pipe->arg;
  return to;
}

// do hard scheduling stuff
pipe_t tmesh_flush(mote_t to)
{
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

  return to->pipe;
}

// chunkize a packet
void tmesh_send(pipe_t pipe, lob_t packet, link_t link)
{
  mote_t to = tmesh_to(pipe);
  if(!to || !packet || !link) return;

  util_chunks_send(to->chunks, packet);
  tmesh_flush(to);
}

mote_t tmesh_free(mote_t mote)
{
  if(!mote) return NULL;
  pipe_free(mote->pipe);
  util_chunks_free(mote->chunks);
  free(mote);
  return NULL;
}

// internal, get or create a mote
mote_t tmesh_mote(net_tmesh_t net, link_t link)
{
  mote_t to;
  if(!net || !link) return LOG("bad args");

  for(to = net->motes; to && to->link != link; to = to->next);
  if(to) return to;

  // create new tmesh pipe/mote
  if(!(to = malloc(sizeof (struct mote_struct)))) return LOG("OOM");
  memset(to,0,sizeof (struct mote_struct));
  if(!(to->chunks = util_chunks_new(0))) return tmesh_free(to);
  if(!(to->pipe = pipe_new("tmesh"))) return tmesh_free(to);
  to->link = link;
  to->net = net;
  to->next = net->motes;
  net->motes = to;

  // set up pipe
  to->pipe->arg = to;
  to->pipe->id = strdup(link->id->hashname);
  to->pipe->send = tmesh_send;

  // TODO make first epochs

  return to;
}


pipe_t tmesh_path(link_t link, lob_t path)
{
  mote_t to;
  net_tmesh_t net;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, "net_tmesh"))) return NULL;
  if(util_cmp("tmesh",lob_get(path,"type"))) return NULL;
//  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
//  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  if(!(to = tmesh_mote(net, link))) return NULL;
  return to->pipe;
}

net_tmesh_t net_tmesh_new(mesh_t mesh, lob_t options)
{
  unsigned int motes;
  net_tmesh_t net;
  
  motes = lob_get_uint(options,"motes");
  if(!motes) motes = 11; // hashtable for active motes

  if(!(net = malloc(sizeof (struct net_tmesh_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_tmesh_struct));

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
//  xht_free(net->motes);
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
    to = (mote_t)pipe->arg;
    if(to->client > 0) close(to->client);
    to->client = client;
  }
  */

}

net_tmesh_t net_tmesh_loop(net_tmesh_t net)
{
  mote_t mote;
  net_tmesh_accept(net);
  for(mote = net->motes;mote && tmesh_flush(mote);mote = mote->next);
  return net;
}

