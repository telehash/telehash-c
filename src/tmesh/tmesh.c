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
//  pipe_free(c->pipe); path may be in mesh->paths 
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

// leave any community
tmesh_t tmesh_leave(tmesh_t tm, cmnty_t c)
{
  cmnty_t i, cn, c2 = NULL;
  if(!tm || !c) return LOG("bad args");
  
  // snip c out
  for(i=tm->coms;i;i = cn)
  {
    cn = i->next;
    if(i==c) continue;
    i->next = c2;
    c2 = i;
  }
  tm->coms = c2;
  
  cmnty_free(c);
  return tm;
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
  
  // receive is on knocked
  if(!k->tx) return tm;

  // construct ping tx frame data
  if(k->mote->ping)
  {
    // copy in ping nonce
    memcpy(k->frame,k->mote->nonce,8);
    // bundle a future rx nonce that we'll wait for next
    mote_seek(k->mote,k->mote->com->medium->min+k->mote->com->medium->max,0,k->mote->nwait);
    k->mote->waiting = 1;
    memcpy(k->frame+8,k->mote->nwait,8);
    // copy in hashname
    memcpy(k->frame+8+8,tm->mesh->id->bin,32);
    // random fill rest
    e3x_rand(k->frame+8+8+32,64-(8+8+32));

     // ciphertext frame after nonce
    chacha20(k->mote->secret,k->mote->nonce,k->frame+8,64-8);

    LOG("PING TX %s",util_hex(k->frame,8,NULL));

    return tm;
  }

  // fill in chunk tx
  int16_t size = util_chunks_size(k->mote->chunks);
  if(size < 0) return LOG("BUG/TODO should never be zero");
  LOG("tx chunk frame size %d",size);

  // TODO, real header, term flag
  k->frame[0] = 0; // stub for now
  k->frame[1] = (uint8_t)size; // max 63
  memcpy(k->frame+2,util_chunks_frame(k->mote->chunks),size);
  // ciphertext full frame
  chacha20(k->mote->secret,k->mote->nonce,k->frame,64);

  return tm;
}


// signal once a knock has been sent/received for this mote
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");
  
  // always set time base to last knock regardless
  k->mote->at = k->start;

  if(!k->actual)
  {
    LOG("knock error");
    return tm;
  }
  
  // use actual time to auto-correct for drift
  k->mote->at = k->actual;

  // tx just changes flags here
  if(k->tx)
  {
    k->mote->sent++;
    LOG("tx done, total %d",k->mote->sent);

    // a sent pong sets this mote free
    if(k->mote->pong) mote_sync(k->mote);
    else util_chunks_next(k->mote->chunks); // advance chunks if any

    return tm;
  }

  // rx knock handling now
  
  // ooh, got a ping eh?
  if(k->mote->ping)
  {
    LOG("PING RX %s",util_hex(k->frame,8,NULL));

    // first decipher frame using given nonce
    chacha20(k->mote->secret,k->frame,k->frame+8,64-8);
    // if this is a link ping, first verify hashname match
    if(k->mote->link && memcmp(k->frame+8+8,k->mote->link->id->bin,32) != 0) return LOG("ping hashname mismatch");

    // cache flag if this was a pong by having the correct nonce we were expecting
    uint8_t pong = (memcmp(k->mote->nonce,k->frame,8) == 0) ? 1 : 0;

    // always sync wait to the bundled nonce for the next pong
    memcpy(k->mote->nonce,k->frame,8);
    memcpy(k->mote->nwait,k->frame+8,8);
    k->mote->waiting = 1;
    k->mote->pong = 1;

    // if it's a public mote, get/create the link mote for the included hashname
    if(k->mote == k->mote->com->public)
    {
      LOG("public %s, checking link mote",pong?"pong":"ping");

      // since there's no ordering w/ public pings, make sure we're inverted to the sender for the pong
      k->mote->order = (k->mote->nonce[7] & 0b00000001) ? 1 : 0;
      
      hashname_t hn = hashname_new(k->frame+8+8);
      if(!hn) return LOG("OOM");
      mote_t mote = tmesh_link(tm, k->mote->com, link_get(tm->mesh, hn->hashname));
      hashname_free(hn);
      if(!mote) return LOG("mote link failed");
      
      // TODO, hint time to accelerate link mote sync process
      mote->at = k->actual;
      // if incoming is a pong, seed = k->mote->nonce and now, else k->mote->nwait and future
      // copy seed into mote when new
    }else{
      LOG("private %s received",pong?"pong":"ping");
      if(pong) mote_sync(k->mote);
    }

    return tm;
  }
  
  // received knock handling now, decipher frame
  chacha20(k->mote->secret,k->mote->nonce,k->frame,64);
  
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
    if(!(m->chunks = util_chunks_new(63))) return mote_free(m);
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
  
  LOG("resetting mote");

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
  m->pong = 0;
  m->waiting = 0;

  return m;
}

uint32_t mote_next(uint8_t *nonce, uint8_t z)
{
  uint32_t next;
  memcpy(&next,nonce,4);
  // smaller for high z, using only high 4 bits of z
  z >>= 4;
  next >>= z;
  return next;
}

// find the first nonce that occurs after this future time of this type
uint64_t mote_seek(mote_t m, uint32_t after, uint8_t tx, uint8_t *nonce)
{
  if(!m || !nonce || !after) return 0;
  
  memcpy(nonce,m->nonce,8);
  uint32_t at = mote_next(nonce,m->z);
  uint8_t atx = ((nonce[7] & 0b00000001) == m->order) ? 0 : 1;
  while(at < after || atx != tx)
  {
    chacha20(m->secret,nonce,nonce,8);
    at += mote_next(nonce,m->z);
    atx = ((nonce[7] & 0b00000001) == m->order) ? 0 : 1;
  }
  
//  LOG("seeking tx %d after %u got %s at %lu",tx,after,util_hex(nonce,8,NULL),at);
  return at;
}

// when is next knock
mote_t mote_knock(mote_t m, knock_t k, uint64_t from)
{
  if(!m || !k || !from) return LOG("bad args");
  if(!m->at) return LOG("paused mote");

  k->mote = m;
  k->start = k->stop = 0;

  while(m->at < from || m->waiting)
  {
    // rotate nonce by ciphering it
    chacha20(m->secret,m->nonce,m->nonce,8);
    m->at += mote_next(m->nonce,m->z);
    // clear wait if matched
    if(m->waiting && memcmp(m->nwait,m->nonce,8) == 0) m->waiting = 0;
    // waiting failsafe
    if(m->at > (from + 1000*1000*100)) return LOG("waiting nonce not found by %lu %lu",m->at,from);
  }

  // set current non-ping channel
  if(!m->ping)
  {
    memset(m->chan,0,2);
    chacha20(m->secret,m->nonce,m->chan,2);
  }

  // least significant nonce bit sets direction
  k->tx = ((m->nonce[7] & 0b00000001) == m->order) ? 0 : 1;

  k->start = m->at;
  k->stop = k->start + (uint64_t)((k->tx) ? m->com->medium->max : m->com->medium->min);

  // when receiving a ping, use wide window to catch more
  if(m->ping) k->stop = k->start + m->com->medium->max;
  
  // derive current channel
  k->chan = m->chan[1] % m->com->medium->chans;

  return m;
}

// initiates handshake over this mote
mote_t mote_sync(mote_t m)
{
  if(!m) return LOG("bad args");

  LOG("synchronizing mote");

  m->pong = 0;

  // handle public beacon motes 
  if(!m->link)
  {
    // TODO intelligent sleepy mode, not just skip some
    m->at += 1000*1000*5; // 5s
    return m;
  }

  // TODO, set up first sync timeout to reset!
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
  LOG("knock compare A %lu",a->start);
  LOG("knock compare B %lu",b->start);
  // any that finish before another starts
  if(a->stop < b->start) return a;
  if(b->stop < a->start) return b;
  // any with link over without
  if(a->mote->link && !b->mote->link) return a;
  if(b->mote->link && !a->mote->link) return b;
  // firstie
  return (a->start < b->start) ? a : b;
}

