#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tmesh.h"

//////////////////
// private community management methods

// find an epoch to send it to in this community
static void cmnty_send(pipe_t pipe, lob_t packet, link_t link)
{
  cmnty_t c;
  if(!pipe || !pipe->arg || !packet || !link)
  {
    LOG("bad args");
    lob_free(packet);
    return;
  }
  c = (cmnty_t)pipe->arg;

  // find link in this community
  // util_chunks_send(to->chunks, packet);
}

static cmnty_t cmnty_free(cmnty_t c)
{
  size_t i;
  if(!c) return NULL;
  // do not free c->name, it's part of c->pipe
  epochs_free(c->ping);
  for(i=0;c->epochs && c->epochs[i];i++) epochs_free(c->epochs[i]);
  pipe_free(c->pipe);
  free(c);
  return NULL;
}

// create a new blank community
static cmnty_t cmnty_new(tmesh_t tm, char *medium, char *name, uint8_t motes)
{
  cmnty_t c;
  uint8_t bin[6];
  if(!tm || !motes || !name || !medium || strlen(medium) < 10) return LOG("bad args");
  if(base32_decode(medium,0,bin,6) != 6) return LOG("bad medium encoding: %s",medium);

  // note, do we need to be paranoid and make sure name is not a duplicate?

  if(!(c = malloc(sizeof (struct cmnty_struct)))) return LOG("OOM");
  memset(c,0,sizeof (struct cmnty_struct));
  memcpy(c->medium,bin,6);
  if(!(c->pipe = pipe_new("tmesh"))) return cmnty_free(c);
  // initialize static pointer arrays
  if(!(c->epochs = malloc((sizeof (epoch_t))*(motes+1)))) return cmnty_free(c);
  memset(c->epochs,0,(sizeof (epoch_t))*(motes+1));
  if(!(c->links = malloc((sizeof (link_t))*(motes+1)))) return cmnty_free(c);
  memset(c->links,0,(sizeof (link_t))*(motes+1));
  c->tm = tm;
  c->next = tm->communities;
  tm->communities = c;

  // set up pipe
  c->pipe->arg = c;
  c->name = c->pipe->id = strdup(name);
  c->pipe->send = cmnty_send;

  return c;
}

// all background processing and soft scheduling, queue up active knocks
static cmnty_t cmnty_loop(tmesh_t tm, cmnty_t c)
{
  if(!tm || !c) return NULL;
  return NULL;
}


//////////////////
// public methods

// join a new private/public community
cmnty_t tmesh_public(tmesh_t tm, char *medium, char *network)
{
  cmnty_t c = cmnty_new(tm,medium,network,NEIGHBORS_MAX);
  if(!c) return LOG("bad args");
  c->type = PUBLIC;
  return c;
}

cmnty_t tmesh_private(tmesh_t tm, char *medium, char *network)
{
  cmnty_t c = cmnty_new(tm,medium,network,NEIGHBORS_MAX);
  if(!c) return LOG("bad args");
  c->type = PRIVATE;
  return c;
}

// add a link known to be in this community to look for
cmnty_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link)
{
  if(!tm || !c || !link) return LOG("bad args");
  // check list of links, add if space and not there
  return c;
}

// attempt to establish a direct connection
cmnty_t tmesh_direct(tmesh_t tm, link_t link, char *medium, uint64_t at)
{
  cmnty_t c;
  if(!link) return LOG("bad args");
  c = cmnty_new(tm,medium,link->id->hashname,1);
  if(!c) return LOG("bad args");
  c->type = DIRECT;
  return c;
}


/*

// per-mote processing and soft scheduling
static mote_t mote_loop(tmesh_t tm, mote_t to)
{
  if(!tm || !to) return NULL;

  // TODO init motes (no link) can only take handshakes for disco/sync, and serve as their epoch time base

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
//  while((packet = util_chunks_receive(to->chunks))) mesh_receive(to->tm->mesh, packet, pipe);

    // TODO if disco is it tx state and was tx'd, switch to rx state and add to list
    // if in rx state and done, rotate tm->disco if there's multiple

  return to;
}



*/

pipe_t tmesh_on_path(link_t link, lob_t path)
{
//  cmnty_t c;
  tmesh_t tm;
//  epoch_t e;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(tm = xht_get(link->mesh->index, "tmesh"))) return NULL;
  if(util_cmp("tmesh",lob_get(path,"type"))) return NULL;
//  if(!(sync = lob_get(path,"sync"))) return LOG("missing sync");
//  e = epoch_new(NULL,NULL);
//  if(!(e = epoch_import(e,sync,link->id->hashname))) return (pipe_t)epoch_free(e);
//  if(!(to = tmesh_mote(tm, link))) return (pipe_t)epoch_free(e);
//  to->syncs = epochs_add(to->syncs, e);
//  if(!to->sync) mote_reset(to); // load new sync epoch immediately if not in sync
//  return to->pipe;
  return NULL;
}

// handle incoming packets for the built-in map channel
void tmesh_map_handler(link_t link, e3x_channel_t chan, void *arg)
{
  lob_t packet;
//  mote_t mote = arg;
  if(!link || !arg) return;

  while((packet = e3x_channel_receiving(chan)))
  {
    LOG("TODO incoming map packet");
    lob_free(packet);
  }

}

// send a map to this mote
void tmesh_map_send(link_t mote)
{
  /* TODO!
  ** send open first if not yet
  ** map packets have len 1 header which is our energy status, then binary frame body of:
  ** >0 energy, body up to 8 epochs (6 bytes) and rssi/status (1 byte) of each, max 56 body
  ** 0 energy, body up to 8 neighbors per packet, 5 bytes of hashname, 1 byte energy, and 1 byte of rssi/status each
  */
}

// new tmesh map channel
lob_t tmesh_on_open(link_t link, lob_t open)
{
  if(!link) return open;
  if(lob_get_cmp(open,"type","map")) return open;
  
  LOG("incoming tmesh map");

  // TODO get matching mote
  // create new channel for this block handler
//  mote->map = link_channel(link, open);
//  link_handle(link,mote->map,tmesh_map_handler,mote);
  
  

  return NULL;
}

tmesh_t tmesh_new(mesh_t mesh, lob_t options)
{
  tmesh_t tm;
  
  if(!(tm = malloc(sizeof (struct tmesh_struct)))) return LOG("OOM");
  memset(tm,0,sizeof (struct tmesh_struct));

  // connect us to this mesh
  tm->mesh = mesh;
  xht_set(mesh->index, "tmesh", tm);
  mesh_on_path(mesh, "tmesh", tmesh_on_path);
  mesh_on_open(mesh, "tmesh_open", tmesh_on_open);
  
  // convenience
//  tm->path = lob_new();
//  lob_set(tm->path,"type","tmesh");
//  mesh->paths = lob_push(mesh->paths, tm->path);

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
tmesh_t tmesh_sync(tmesh_t tm, char *medium)
{
  epoch_t sync;
  uint8_t bin[6];
  if(!tm || !medium || strlen(medium) < 10) return LOG("bad args");
  if(base32_decode(medium,0,bin,6) != 6) return LOG("bad medium encoding: %s",medium);
  if(!(sync = epoch_new(tm->mesh,bin))) return LOG("epoch error");
  // use our hashname as default secret for syncs
  memcpy(sync->secret,tm->mesh->id->bin,32);
//  tm->syncs = epochs_add(tm->syncs,sync);
  return tm;
}

/*
// become discoverable by anyone with this epoch id, pass NULL to reset all
tmesh_t tmesh_discoverable(tmesh_t tm, char *medium, char *network)
{
  uint8_t bin[6];
  epoch_t disco;
  if(!tm) return NULL;
  
  // no medium resets
  if(!medium)
  {
    tm->disco = epochs_free(tm->disco);
    tm->dim = lob_free(tm->dim);
    return tm;
  }

  if(strlen(medium) < 10 || base32_decode(medium,0,bin,6) != 6) return LOG("bad medium encoding: %s",medium);

  // new discovery epoch
  if(!(disco = epoch_new(tm->mesh,bin))) return LOG("epoch error");

  // network is discovery secret
  if(network) e3x_hash((uint8_t*)network,strlen(network),disco->secret);
  else e3x_hash((uint8_t*)"telehash",8,disco->secret);

  tm->disco = epochs_add(tm->disco,disco);
  
  // generate discovery intermediate keys packet
  tm->dim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));

  return tm;
}
*/

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
  cmnty_t c;
  if(!tm) return LOG("bad args");

  // rx list cleared since it is rebuilt by the mote loop
  tm->rx = NULL; 

  // each cmnty loop will check and process data
  // will also check if the epoch was a sync one and add that rx, else normal rx
  for(c = tm->communities;c && cmnty_loop(tm, c);c = c->next);

  // if active had a buffer and was my sync channel, my own sync rx is reset/added
  
  return tm;
}


// all devices
radio_t radio_devices[RADIOS_MAX] = {0};

radio_t radio_device(radio_t device)
{
  int i;
  for(i=0;i<RADIOS_MAX;i++)
  {
    if(radio_devices[i]) continue;
    radio_devices[i] = device;
    return device;
  }
  return NULL;
}


// return the next hard-scheduled epoch from this given point in time
epoch_t radio_next(radio_t device, tmesh_t tm, uint64_t from)
{
  // find nearest tx
  //  check if any rx can come first
  // take first rx
  //  check if current disco can fit first
  // pop off list and set active
  return NULL;
}
