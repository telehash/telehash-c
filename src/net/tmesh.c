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

// per-mote processing and soft scheduling
static pipe_t mote_loop(net_tmesh_t net, mote_t to)
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
//  tmesh_flush(to);
}

mote_t tmesh_free(mote_t mote)
{
  if(!mote) return NULL;
  pipe_free(mote->pipe);
  util_chunks_free(mote->chunks);
  free(mote);
  return NULL;
}

// get or create a mote
mote_t net_tmesh_mote(net_tmesh_t net, link_t link)
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
  if(!(to = net_tmesh_mote(net, link))) return NULL;
  return to->pipe;
}

net_tmesh_t net_tmesh_new(mesh_t mesh, lob_t options, epoch_t (*init)(net_tmesh_t net, epoch_t e))
{
  unsigned int motes;
  net_tmesh_t net;
  
  if(!init) return LOG("requires epoch phy init handler");

  motes = lob_get_uint(options,"motes");
  if(!motes) motes = 11; // hashtable for active motes

  if(!(net = malloc(sizeof (struct net_tmesh_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_tmesh_struct));
  net->init = init;

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

// add a sync epoch from this header
net_tmesh_t net_tmesh_sync(net_tmesh_t net, char *header)
{
  epoch_t sync;
  if(!net || !header || strlen(header) < 13) return LOG("bad args");
  if(!(sync = epoch_new(NULL))) return LOG("OOM");
  if(!epoch_import2(sync,header,net->mesh->id->hashname)) return (net_tmesh_t)epoch_free(sync);
  if(!net->init(net, sync)) return (net_tmesh_t)epoch_free(sync);
  net->syncs = epochs_add(net->syncs,sync);
  return net;
}


/* discussion on flow

* my sync is a virtual mote that is constantly reset
* each mote has it's own time sync base to derive window counters
* any mote w/ a tx knock ready is priority
* upon tx, my sync rx is reset
* when no tx, the next rx is always scheduled
* when no tx for some period of time, generate one on the sync epoch
* when in discovery mode, it is a virtual mote that is always tx'ing if nothing else is

*/

net_tmesh_t net_tmesh_loop(net_tmesh_t net)
{
  mote_t mote;
  if(!net) return LOG("bad args");

  // rx list cleared since it is rebuilt by the mote loop
  net->rx = NULL; 

  // each mote loop will check if it was the active one and process data
  // will also check if the epoch was a sync one and add that rx, else normal rx
  for(mote = net->motes;mote && mote_loop(net, mote);mote = mote->next);

  // if active had a buffer and was my sync channel, my own sync rx is reset/added

  // if any active, is force cleared
  net->active = NULL;

  return net;
}

// return the next hard-scheduled epoch from this given point in time
epoch_t net_tmesh_next(net_tmesh_t net, uint64_t from)
{
  // find nearest tx
  //  check if any rx can come first
  // take first rx
  //  check if any rx can come first
  // pop off list and set active
  return NULL;
}

