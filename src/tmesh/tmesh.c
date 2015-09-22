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
  cmnty_t c;
  mote_t m;
  if(!pipe || !pipe->arg || !packet || !link)
  {
    LOG("bad args");
    lob_free(packet);
    return;
  }
  c = (cmnty_t)pipe->arg;

  // find link in this community w/ tx epoch
  for(m=c->motes;m;m=m->next) if(m->link == link)
  {
    util_chunks_send(m->chunks, packet);
    return;
  }

  LOG("no link in community");
  lob_free(packet);
}

static cmnty_t cmnty_free(cmnty_t c)
{
  if(!c) return NULL;
  // do not free c->name, it's part of c->pipe
  // TODO free motes
  pipe_free(c->pipe);
  free(c);
  return NULL;
}

// create a new blank community
static cmnty_t cmnty_new(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c;
  uint8_t bin[6];
  if(!tm || !name || !medium || strlen(medium) < 10) return LOG("bad args");
  if(base32_decode(medium,0,bin,6) != 6) return LOG("bad medium encoding: %s",medium);
  if(!medium_check(tm,bin)) return LOG("unknown medium %s",medium);

  // note, do we need to be paranoid and make sure name is not a duplicate?

  if(!(c = malloc(sizeof (struct cmnty_struct)))) return LOG("OOM");
  memset(c,0,sizeof (struct cmnty_struct));

  if(!(c->medium = medium_get(tm,bin))) return cmnty_free(c);
  if(!(c->pipe = pipe_new("tmesh"))) return cmnty_free(c);
  c->tm = tm;
  c->next = tm->coms;
  tm->coms = c;

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
  epoch_t ping, echo;
  uint8_t roll[64];
  cmnty_t c = cmnty_new(tm,medium,name);
  if(!c) return LOG("bad args");
  c->type = PUBLIC;
  
  if(!(c->sync = mote_new(NULL))) return cmnty_free(c);
  if(!(ping = c->sync->epochs = epoch_new(1))) return cmnty_free(c);
  if(!(echo = ping->next = epoch_new(1))) return cmnty_free(c);
  ping->type = PING;
  echo->type = ECHO;
  e3x_hash(c->medium->bin,6,roll);
  e3x_hash((uint8_t*)name,strlen(name),roll+32);
  e3x_hash(roll,64,ping->secret);

  // cache ping channel for faster detection
  mote_knock(c->sync,c->medium,0);
  c->sync->ping = c->sync->kchan;

  // generate public intermediate keys packet
  if(!tm->pubim) tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));

  return c;
}

cmnty_t tmesh_private(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c = cmnty_new(tm,medium,name);
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
mote_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link)
{
  uint8_t roll[64];
  mote_t m;
  if(!tm || !c || !link) return LOG("bad args");

  // check list of motes, add if not there
  for(m=c->motes;m;m = m->next) if(m->link == link) return m;

  if(!(m = mote_new(link))) return LOG("OOM");
  if(!(m->epochs = epoch_new(0))) return mote_free(m);
  m->epochs->type = PING;
  m->next = c->motes;
  c->motes = m;
  
  // generate mote-specific ping secret by combining community one with it
  memcpy(roll,c->sync->epochs->secret,32);
  memcpy(roll+32,link->id->bin,32);
  e3x_hash(roll,64,m->epochs->secret);
  mote_knock(m,c->medium,0);
  m->ping = m->kchan;

  return m;
}

// attempt to establish a direct connection
cmnty_t tmesh_direct(tmesh_t tm, link_t link, char *medium, uint64_t at)
{
  cmnty_t c;
  if(!link) return LOG("bad args");
  c = cmnty_new(tm,medium,link->id->hashname);
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
  if(!mesh) return NULL;

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
  cmnty_t c, next;
  if(!tm) return;
  for(c=tm->coms;c;c=next)
  {
    next = c->next;
    cmnty_free(c);
  }
  lob_free(tm->pubim);
  free(tm);
  return;
}

tmesh_t tmesh_loop(tmesh_t tm)
{
  cmnty_t c;
  lob_t packet;
  mote_t m;
  if(!tm) return LOG("bad args");

  // process any packets into mesh_receive
  for(c = tm->coms;c;c = c->next) 
    for(m=c->motes;m;m=m->next)
      while((packet = util_chunks_receive(m->chunks)))
        mesh_receive(tm->mesh, packet, c->pipe); // TODO associate mote for neighborhood
  
  return tm;
}

// all devices
radio_t radio_devices[RADIOS_MAX] = {0};

// validate medium by checking energy
uint32_t medium_check(tmesh_t tm, uint8_t medium[6])
{
  int i;
  uint32_t energy;
  for(i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if((energy = radio_devices[i]->energy(tm, medium))) return energy;
  }
  return 0;
}

// get the full medium
medium_t medium_get(tmesh_t tm, uint8_t medium[6])
{
  int i;
  medium_t m;
  // get the medium from a device
  for(i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if((m = radio_devices[i]->get(tm,medium))) return m;
  }
  return NULL;
}

radio_t radio_device(radio_t device)
{
  int i;
  for(i=0;i<RADIOS_MAX;i++)
  {
    if(radio_devices[i]) continue;
    radio_devices[i] = device;
    device->id = i;
    return device;
  }
  return NULL;
}

radio_t radio_prep(radio_t device, tmesh_t tm, uint64_t from)
{
  cmnty_t com;
  mote_t mote;
  if(!tm || !device) return LOG("bad args");

  for(com=tm->coms;com;com=com->next)
  {
    if(com->medium->radio != device->id) continue;
    // TODO check c->sync ping/echo
    // check every mote
    for(mote=com->motes;mote;mote=mote->next)
    {
      mote_knock(mote,com->medium,from);
      
    }
  }
  
  return device;
}


// return the next hard-scheduled mote for this radio
mote_t radio_get(radio_t device, tmesh_t tm)
{
  cmnty_t com;
  mote_t mote, tx, rx;
  uint32_t win, lwin;
  if(!tm || !device) return LOG("bad args");

  tx = rx = NULL;
  for(com=tm->coms;com;com=com->next)
  {
    if(com->medium->radio != device->id) continue;
    for(mote = com->motes;mote;mote = mote->next)
    {
      if(!mote->kstate == READY) continue;
      // TX > RX
      // LINK > PAIR > ECHO > PING
      if(mote->knock->dir == TX)
      {
        if(!tx || tx->knock->type > mote->knock->type || tx->kstart < mote->kstart) tx = mote;
      }else{
        if(!rx || rx->knock->type > mote->knock->type || rx->kstart < mote->kstart) rx = mote;
      }
      
    }
  }

  mote = tx?tx:rx;
  if(!mote) return NULL;

  // TODO keep best TX, take best RX that fits ahead
  // mode to expand-fill RX for faster pairing or when powered
  
  win = ((mote->kstart - mote->knock->base) / EPOCH_WINDOW);
  lwin = util_sys_long(win);
  memset(device->frame,0,8+64);
  memcpy(device->frame,&lwin,4); // nonce area

  // TODO copy in tx data

  // ciphertext frame
  chacha20(mote->knock->secret,device->frame,device->frame+8,64);

  return mote;
}

// signal once a frame has been sent/received for this mote
radio_t radio_done(radio_t device, tmesh_t tm, mote_t mote)
{
  if(mote->kstate == READY) return LOG("knock not done");
  if(mote->kstate == SKIP) return device; // do nothing
  
  switch(mote->knock->type)
  {
    case PING:
      // if RX generate echo TX
      break;
    case ECHO:
      // if RX then create pair
      break;
    case PAIR:
      break;
    case LINK:
      break;
    default :
      break;
  }
  
  if(mote->knock->dir == TX)
  {
    // TODO check priv coms match device and ping->kchan
    // set up echo RX
  }
  
  return device;
}
