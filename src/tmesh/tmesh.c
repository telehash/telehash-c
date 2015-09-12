#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

//////////////////
// private community management methods

// find an epoch to send it to in this community
static void cmnty_send(pipe_t pipe, lob_t packet, link_t link)
{
  size_t i;
  cmnty_t c;
  if(!pipe || !pipe->arg || !packet || !link)
  {
    LOG("bad args");
    lob_free(packet);
    return;
  }
  c = (cmnty_t)pipe->arg;

  // find link in this community w/ tx epoch
  for(i=0;c->links[i];i++) if(c->links[i] == link && epoch_knock(c->epochs[i],1))
  {
    util_chunks_send(c->epochs[i]->knock->chunks, packet);
    return;
  }

  LOG("no link in community");
  lob_free(packet);
}

static cmnty_t cmnty_free(cmnty_t c)
{
  size_t i;
  if(!c) return NULL;
  // do not free c->name, it's part of c->pipe
  epochs_free(c->ping);
  if(c->epochs)
  {
    for(i=0;c->epochs[i];i++) epochs_free(c->epochs[i]);
    free(c->epochs);
    free(c->links);
  }
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
  if(!radio_energy(tm->mesh,bin)) return LOG("unknown medium %s",medium);

  // note, do we need to be paranoid and make sure name is not a duplicate?

  if(!(c = malloc(sizeof (struct cmnty_struct)))) return LOG("OOM");
  memset(c,0,sizeof (struct cmnty_struct));

  if(!(c->medium = radio_medium(tm->mesh,bin))) return cmnty_free(c);
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


//////////////////
// public methods

// join a new private/public community
cmnty_t tmesh_public(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c = cmnty_new(tm,medium,name,NEIGHBORS_MAX);
  if(!c) return LOG("bad args");
  c->type = PUBLIC;

  // generate public intermediate keys packet
  if(!tm->pubim) tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));

  return c;
}

cmnty_t tmesh_private(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c = cmnty_new(tm,medium,name,NEIGHBORS_MAX);
  if(!c) return LOG("bad args");
  c->type = PRIVATE;
  c->pipe->path = lob_new();
  lob_set(c->pipe->path,"type","tmesh");
  lob_set(c->pipe->path,"medium",medium);
  lob_set(c->pipe->path,"name",name);
  tm->mesh->paths = lob_push(tm->mesh->paths, c->pipe->path);
  return c;
}

// add a link known to be in this community to look for
cmnty_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link)
{
  epoch_t e;
  size_t i;
  if(!tm || !c || !link) return LOG("bad args");

  // check list of links, add if space and not there
  for(i=0;c->links[i];i++) if(c->links[i] == link) return c;
  
  if(i == c->motes) return DEBUG("community full");

  if(!(e = epoch_new(tm->mesh, c->medium))) return NULL;
  e->type = PING;
  c->links[i] = link;
  c->epochs[i] = e;

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


pipe_t tmesh_on_path(link_t link, lob_t path)
{
//  cmnty_t c;
  tmesh_t tm;
//  epoch_t e;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(tm = xht_get(link->mesh->index, "tmesh"))) return NULL;
  if(util_cmp("tmesh",lob_get(path,"type"))) return NULL;
  // TODO, check for community match and add
  // or create direct?
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
  lob_t packet;
  size_t i;
  epoch_t e;
  if(!tm) return LOG("bad args");

  // process any packets into mesh_receive
  for(c = tm->communities;c;c = c->next) 
    for(i=0;c->epochs[i];i++) 
    for(e=c->epochs[i];e;e = e->next)
    if(e->knock)
    {
      while((packet = util_chunks_receive(e->knock->chunks)))
        mesh_receive(tm->mesh, packet, c->pipe);
    }
  
  return tm;
}

tmesh_t tmesh_pre(tmesh_t tm)
{
  cmnty_t c;
  size_t i;
  epoch_t e;
  if(!tm) return LOG("bad args");

  // clean state
  tm->tx = tm->rx = tm->any = NULL;
  
  // queue up any active knocks anywhere
  for(c = tm->communities;c;c = c->next)
  {
    // TODO check c->ping/echo for tm->any
    // check every epoch
    for(i=0;c->epochs[i];i++) for(e=c->epochs[i];e;e = e->next) if(e->knock)
    {
      // check for a chunk waiting and add to tx
      // else add to rx
    }
  }

  return tm;
}

tmesh_t tmesh_post(tmesh_t tm, epoch_t e)
{
  if(!tm) return LOG("bad args");

  // TODO process whatever e was
  // if ping/echo, handle special cases directly here
  
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

// validate medium by checking energy
uint32_t radio_energy(mesh_t m, uint8_t medium[6])
{
  int i;
  uint32_t energy;
  for(i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if((energy = radio_devices[i]->energy(m, medium))) return energy;
  }
  return 0;
}

// get the full medium
medium_t radio_medium(mesh_t mesh, uint8_t medium[6])
{
  int i;
  medium_t m;
  // get the medium from a device
  for(i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if((m = radio_devices[i]->get(mesh,medium))) return m;
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
