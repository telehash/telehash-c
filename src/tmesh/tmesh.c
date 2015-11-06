#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

//////////////////
// private community management methods

// find a mote to send it to in this community
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

  // find link in this community
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
  uint8_t bin[5];
  if(!tm || !name || !medium || strlen(medium) != 8) return LOG("bad args");
  if(base32_decode(medium,0,bin,5) != 5) return LOG("bad medium encoding: %s",medium);
  if(!medium_check(tm,bin)) return LOG("unknown medium %s",medium);

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
cmnty_t tmesh_join(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c = cmnty_new(tm,medium,name);
  if(!c) return LOG("bad args");

  if(medium_public(c->medium))
  {
    // generate the public shared ping mote
    c->public = c->motes = mote_new(NULL);
    c->public->com = c;
    mote_reset(c->public);
    c->public->at = 1; // enable to start

    // generate public intermediate keys packet
    if(!tm->pubim) tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
    
  }else{
    c->pipe->path = lob_new();
    lob_set(c->pipe->path,"type","tmesh");
    lob_set(c->pipe->path,"medium",medium);
    lob_set(c->pipe->path,"name",name);
    tm->mesh->paths = lob_push(tm->mesh->paths, c->pipe->path);
    
  }
  

  return c;
}

// add a link known to be in this community to look for
mote_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link)
{
  mote_t m;
  if(!tm || !c || !link) return LOG("bad args");

  // check list of motes, add if not there
  for(m=c->motes;m;m = m->next) if(m->link == link) return m;

  if(!(m = mote_new(link))) return LOG("OOM");
  m->next = c->motes;
  c->motes = m;
  m->com = c;
  
  return mote_reset(m);
}

pipe_t tmesh_on_path(link_t link, lob_t path)
{
  tmesh_t tm;

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
  ** one channel per community w/ open containing medium/name
  ** map packets use cbor, array of neighbors, 6 bytes hashname, 1 byte z, 1 byte rssi/status, 8 bytes nonce, 4 bytes last knock
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
  tm->sort = knock_sooner;
  
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
uint32_t medium_check(tmesh_t tm, uint8_t medium[5])
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
medium_t medium_get(tmesh_t tm, uint8_t medium[5])
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

uint8_t medium_public(medium_t m)
{
  if(m && m->bin[0] > 128) return 0;
  return 1;
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


tmesh_t tmesh_knock(tmesh_t tm, knock_t k, uint64_t from, radio_t device)
{
  cmnty_t com;
  mote_t mote;
  struct knock_struct ktmp;
  if(!tm || !k) return LOG("bad args");

  memset(k,0,sizeof(struct knock_struct));
  memset(&ktmp,0,sizeof(struct knock_struct));
  for(com=tm->coms;com;com=com->next)
  {
    if(device && com->medium->radio != device->id) continue;
    // check every mote
    for(mote=com->motes;mote;mote=mote->next)
    {
      if(!mote_knock(mote,&ktmp,from)) continue;
      // if active tx and we don't have any data, skip
      if(ktmp.tx && !mote->ping && util_chunks_size(mote->chunks) < 0) continue;
      // use the new one if preferred
      if(!k->mote || tm->sort(k,&ktmp) != k)
      {
        memcpy(k,&ktmp,sizeof(struct knock_struct));
      }
    }
  }
  
  // make sure a knock was found
  if(!k->mote) return NULL;
  
  // fill in tx frame data
  if(k->tx)
  {
    if(k->mote->ping)
    {
      // if not sending a pong, rotate nonce until next one is rx for a pong
      if(!k->mote->pong)
      {
        uint8_t next[8], max=16;
        while(max--)
        {
          memcpy(next,k->mote->nonce,8);
          chacha20(k->mote->secret,k->mote->nonce,next,8);
          if((k->mote->nonce[3]%2) == k->mote->order) break;
          memcpy(k->mote->nonce,next,8);
        }
      }
      // copy in nonce, hashname and random
      memcpy(k->frame,k->mote->nonce,8);
      memcpy(k->frame+8,tm->mesh->id->bin,32);
      e3x_rand(k->frame+8+32,24);
    }else{
      int16_t size = util_chunks_size(k->mote->chunks);
      if(size >= 0)
      {
        // TODO, real header, term flag
        k->frame[0] = 0; // stub for now
        k->frame[1] = (uint8_t)size;
        memcpy(k->frame+2,util_chunks_frame(k->mote->chunks),size);
      }
    }

    // ciphertext frame
    chacha20(k->mote->secret,k->mote->nonce,k->frame,64);
  }

  return tm;
}


// signal once a knock has been sent/received for this mote
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k)
{
  cmnty_t com;
  mote_t mote;
  if(!tm || !k) return LOG("bad args");
  
  // always set time base to last knock regardless
  k->mote->at = k->start;

  if(!k->actual)
  {
    LOG("knock error");
    return tm;
  }

  // convenience
  com = k->mote->com;
  
  // tx stats are always valid
  if(k->tx) k->mote->sent++;

  // decipher frame
  chacha20(k->mote->secret,k->mote->nonce,k->frame,64);
  
  // handle any ping first
  if(k->mote->ping)
  {
    // if we received a ping, cache the link
    if(!k->tx)
    {
      // if it's the public ping, cache the incoming hashname as a link
      if(k->mote == com->public)
      {
        link_t link;
        hashname_t hn = hashname_new(k->frame+8);
        if(hn && (link = link_get(tm->mesh, hn->hashname)))
        {
          // TODO, clean-up any previous stale link
          k->mote->link = link;
        }
        hashname_free(hn);
        // also set our order to opposite of theirs since they just tx'd and there's no natural order
        k->mote->order = (k->frame[3]%2) ? 1 : 0;
      }

      // always in pong status after any ping rx
      k->mote->pong = 1;

      // if the nonce doesn't match, we use it and pong next tx
      if(memcmp(k->frame,k->mote->nonce,8) != 0)
      {
        memcpy(k->mote->nonce,k->frame,8);
        return tm;
      }
      // otherwise continue to pong handler
    }

    // when in pong status
    if(k->mote->pong)
    {
      // if public pong, create/rebase mote from cached link
      if(k->mote == com->public)
      {
        // check for existing mote already or make new
        for(mote = com->motes;mote;mote = mote->next) if(mote->link == k->mote->link) break;
        if(!mote && !(mote = tmesh_link(tm, com, k->mote->link))) return LOG("internal error");
        // public ping is back in open mode
        k->mote->link = NULL;
        k->mote->pong = 0;
      }else{
        mote = k->mote;
      }
      // force reset and use given nonce seed to start
      mote_reset(mote);
      memcpy(mote->nonce,k->frame,8);
      mote_link(mote);
    }

    return tm;
  }
  
  // transmitted knocks handle first
  if(k->tx)
  {
    // advance chunking now
    util_chunks_next(k->mote->chunks);
    return tm;
  }
  
  // received knock handling now

  // TODO check and validate frame[0] first
  k->mote->at = k->actual; // self-correcting sync based on exact rx time

  // received stats only after minimal validation
  k->mote->received++;

  // process incoming chunk to link
  util_chunks_read(k->mote->chunks,k->frame+1,k->frame[1]+1);

  return tm;
}

mote_t mote_new(link_t link)
{
  uint8_t i;
  mote_t m;
  
  if(!(m = malloc(sizeof(struct mote_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct mote_struct));

  // determine ordering based on hashname compare
  if(link)
  {
    if(!(m->chunks = util_chunks_new(64))) return mote_free(m);
    m->link = link;
    for(i=0;i<32;i++)
    {
      if(link->id->bin[i] == link->mesh->id->bin[i]) continue;
      m->order = (link->id->bin[i] > link->mesh->id->bin[i]) ? 1 : 0;
      break;
    }
  }

  return m;
}

mote_t mote_free(mote_t m)
{
  if(!m) return NULL;
  util_chunks_free(m->chunks);
  free(m);
  return NULL;
}

mote_t mote_reset(mote_t m)
{
  uint8_t *a, *b, roll[64];
  uint8_t zeros[32] = {0};
  if(!m) return LOG("bad args");
  
  // reset stats
  m->sent = m->received = 0;

  // generate mote-specific secret, roll up community first
  e3x_hash(m->com->medium->bin,5,roll);
  e3x_hash((uint8_t*)(m->com->name),strlen(m->com->name),roll+32);
  e3x_hash(roll,64,m->secret);

  // no link, uses zeros
  if(!m->link)
  {
    a = b = zeros;
  }else{
    m->chunks = util_chunks_free(m->chunks);
    // TODO detach pipe from link
    // add both hashnames in order
    if(m->order)
    {
      a = m->link->id->bin;
      b = m->link->mesh->id->bin;
    }else{
      b = m->link->id->bin;
      a = m->link->mesh->id->bin;
    }
  }
  memcpy(roll,m->secret,32);
  memcpy(roll+32,a,32);
  e3x_hash(roll,64,m->secret);
  memcpy(roll,m->secret,32);
  memcpy(roll+32,b,32);
  e3x_hash(roll,64,m->secret);
  
  // initalize chan based on secret w/ zero nonce
  memset(m->chan,0,2);
  memset(m->nonce,0,8);
  chacha20(m->secret,m->nonce,m->chan,2);
  
  // randomize nonce
  e3x_rand(m->nonce,8);
  
  // always in ping mode after reset and default z
  m->ping = 1;
  m->z = m->com->medium->z;

  return m;
}

uint32_t mote_next(mote_t m)
{
  if(!m) return 0;
  uint32_t next;
  uint8_t z = m->z >> 4; // only high 4 bits
  memcpy(&next,m->nonce,4);
  return next >> z; // smaller for high z
}

// advance one window forward
mote_t mote_window(mote_t m)
{
  uint8_t len = 8;
  if(!m) return LOG("bad args");

  // rotate nonce by ciphering it
  chacha20(m->secret,m->nonce,m->nonce,len);
  // add new time to base
  m->at += mote_next(m);

  // get current non-ping channel
  if(!m->ping)
  {
    memset(m->chan,0,2);
    chacha20(m->secret,m->nonce,m->chan,2);
  }

  return m;
}

// when is next knock
mote_t mote_knock(mote_t m, knock_t k, uint64_t from)
{
  uint32_t next;
  if(!m || !k || !from || !m->at) return 0;

  k->mote = m;
  next = mote_next(m);
  if((m->at+next) > from)
  {
    // least significant nonce byte sets direction
    k->tx = m->tx = ((m->nonce[7]%2) == m->order) ? 0 : 1;

    k->start = m->at+next;
    k->stop = k->start + (uint64_t)((k->tx) ? m->com->medium->max : m->com->medium->min);

    // when receiving a ping, use wide window to catch more
    if(m->ping) k->stop = k->start + m->com->medium->max;
    
    // derive current channel
    k->chan = m->chan[1] % m->com->medium->chans;

    return m;
  }
  // tail recurse to step until the next future one
  return mote_knock(mote_window(m), k, from);
}

// initiates handshake over this mote
mote_t mote_link(mote_t m)
{
  if(!m || !m->link) return LOG("bad args");
  m->ping = 0;
  m->chunks = util_chunks_free(m->chunks);
  m->chunks = util_chunks_new(63);
  // establish the pipe path
  link_pipe(m->link,m->com->pipe);
  // if public and no keys, send discovery
  if(m->com->tm->pubim && !m->link->x)
  {
    m->com->pipe->send(m->com->pipe, lob_copy(m->com->tm->pubim), m->link);
    return m;
  }
  // trigger a new handshake over it
  lob_free(link_resync(m->link));
  return m;
}

knock_t knock_sooner(knock_t a, knock_t b)
{
  LOG("knock compare %lld <=> %lld",a->start,b->start);
  return (a->start < b->start) ? a : b;
}

