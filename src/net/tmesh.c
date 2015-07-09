#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tmesh.h"


// just make sure it's connected
mote_t mote_to(pipe_t pipe)
{
  mote_t to;
  if(!pipe || !pipe->arg) return NULL;
  to = (mote_t)pipe->arg;
  return to;
}

// per-mote processing and soft scheduling
static mote_t mote_loop(tmesh_t tm, mote_t to)
{
  if(!tm || !to) return NULL;

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
//  while((packet = util_chunks_receive(to->chunks))) mesh_receive(to->tm->mesh, packet, pipe);

  return to;
}

// just chunkize a packet, next loop will load it
static void mote_send(pipe_t pipe, lob_t packet, link_t link)
{
  mote_t to = mote_to(pipe);
  if(!to || !packet) return;

  util_chunks_send(to->chunks, packet);
}

static mote_t mote_free(mote_t mote)
{
  if(!mote) return NULL;
  epochs_free(mote->active);
  epochs_free(mote->syncs);
  pipe_free(mote->pipe);
  util_chunks_free(mote->chunks);
  free(mote);
  return NULL;
}

// reset a mote to unsynchronized state
static mote_t mote_reset(mote_t mote)
{
  epochs_t syncs;
  epoch_t e;
  size_t i;
  if(!mote) return NULL;
  
  mote->sync = 0;

  // free any old active epochs
  mote->active = epochs_free(mote->active);
  
  // copy in any per-mote sync epochs or use global mesh ones
  syncs = mote->syncs ? mote->syncs : mote->tm->syncs;
  for(i=0;epochs_index(syncs,i);i++)
  {
    e = epoch_new(epoch_id(epochs_index(syncs,i)));
    epoch_import(e,NULL,mote->link->id->hashname); // make sure correct sync body
    mote->active = epochs_add(mote->active,e);
  }
  
  return mote;
}

// new initialized epoch
static epoch_t tmesh_epoch(tmesh_t tm, char *id)
{
  epoch_t e;
  if(!tm || !id) return LOG("bad args");
  if(!(e = epoch_new(id))) return LOG("OOM");
  if(!tm->init(tm, e)) return epoch_free(e);
  return e;
}

// get or create a mote
mote_t tmesh_mote(tmesh_t tm, link_t link)
{
  mote_t to;
  if(!tm || !link) return LOG("bad args");

  for(to = tm->motes; to && to->link != link; to = to->next);
  if(to) return to;

  // create new tmesh pipe/mote
  if(!(to = malloc(sizeof (struct mote_struct)))) return LOG("OOM");
  memset(to,0,sizeof (struct mote_struct));
  if(!(to->chunks = util_chunks_new(0))) return mote_free(to);
  if(!(to->pipe = pipe_new("tmesh"))) return mote_free(to);
  to->link = link;
  to->tm = tm;
  to->next = tm->motes;
  tm->motes = to;

  // set up pipe
  to->pipe->arg = to;
  to->pipe->id = strdup(link->id->hashname);
  to->pipe->send = mote_send;

  return mote_reset(to);
}


pipe_t tmesh_path(link_t link, lob_t path)
{
  mote_t to;
  tmesh_t tm;
  char *sync;
  epoch_t e;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(tm = xht_get(link->mesh->index, "tmesh"))) return NULL;
  if(util_cmp("tmesh",lob_get(path,"type"))) return NULL;
  if(!(sync = lob_get(path,"sync"))) return LOG("missing sync");
  e = epoch_new(NULL);
  if(!(e = epoch_import(e,sync,link->id->hashname))) return (pipe_t)epoch_free(e);
  if(!(to = tmesh_mote(tm, link))) return (pipe_t)epoch_free(e);
  to->syncs = epochs_add(to->syncs, e);
  if(!to->sync) mote_reset(to); // load new sync epoch immediately if not in sync
  return to->pipe;
}

tmesh_t tmesh_new(mesh_t mesh, lob_t options, epoch_t (*init)(tmesh_t tm, epoch_t e))
{
  unsigned int motes;
  tmesh_t tm;
  
  if(!init) return LOG("requires epoch phy init handler");

  motes = lob_get_uint(options,"motes");
  if(!motes) motes = 11; // hashtable for active motes

  if(!(tm = malloc(sizeof (struct tmesh_struct)))) return LOG("OOM");
  memset(tm,0,sizeof (struct tmesh_struct));
  tm->init = init;

  // connect us to this mesh
  tm->mesh = mesh;
  xht_set(mesh->index, "tmesh", tm);
  mesh_on_path(mesh, "tmesh", tmesh_path);
  
  // convenience
  tm->path = lob_new();
  lob_set(tm->path,"type","tmesh");
  mesh->paths = lob_push(mesh->paths, tm->path);

  return tm;
}

void tmesh_free(tmesh_t tm)
{
  if(!tm) return;
//  xht_free(tm->motes);
//  lob_free(tm->path); // managed by mesh->paths
  free(tm);
  return;
}

// add a sync epoch from this header
tmesh_t tmesh_sync(tmesh_t tm, char *header)
{
  epoch_t sync;
  if(!tm || !header || strlen(header) < 13) return LOG("bad args");
  if(!(sync = tmesh_epoch(tm,header))) return LOG("OOM");
  if(!epoch_import(sync,header,tm->mesh->id->hashname)) return (tmesh_t)epoch_free(sync);
  tm->syncs = epochs_add(tm->syncs,sync);
  return tm;
}

// become discoverable by anyone with this epoch id, pass NULL to reset all
tmesh_t tmesh_discoverable(tmesh_t tm, char *id)
{
  epoch_t e;
  knock_t k;
  if(!tm) return NULL;
  if(!id)
  {
    tm->disco = knock_free_next(tm->disco);
    return tm;
  }
  
  // temporary epoch
  if(!(e = tmesh_epoch(tm,id))) return NULL;
  
  // verify the epoch is for a cipher set we support
  // TODO add tx buffer of lob IM
  k = knock_new(1);
  epoch_knock(e,k,1); // initialize to window 0 only
  epoch_free(e);
  k->next = tm->disco;
  tm->disco = k;

  return tm;
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

tmesh_t tmesh_loop(tmesh_t tm)
{
  mote_t mote;
  if(!tm) return LOG("bad args");

  // rx list cleared since it is rebuilt by the mote loop
  tm->rx = NULL; 

  // each mote loop will check if it was the active one and process data
  // will also check if the epoch was a sync one and add that rx, else normal rx
  for(mote = tm->motes;mote && mote_loop(tm, mote);mote = mote->next);

  // if active had a buffer and was my sync channel, my own sync rx is reset/added

  // discovery special processing
  if(tm->disco)
  {
    // TODO if we just tx'd a disco knock, we should rx sequence 1, and rx sequence 0 if we can before then
    // else add a tx
  }

  // if any active, is force cleared
  tm->active = NULL;

  return tm;
}

// return the next hard-scheduled epoch from this given point in time
epoch_t tmesh_next(tmesh_t tm, uint64_t from)
{
  // find nearest tx
  //  check if any rx can come first
  // take first rx
  //  check if any rx can come first
  // pop off list and set active
  return NULL;
}

