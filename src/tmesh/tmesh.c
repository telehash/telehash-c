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
    c->public->at = 1; // enabled by default, TODO make recent to avoid window catchup
    c->public->public = 1;

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
  for(m=c->motes;m;m = m->next) if(m->link == link && !m->public) return m;

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
void tmesh_map_handler(link_t link, chan_t chan, void *arg)
{
  lob_t packet;
//  mote_t mote = arg;
  if(!link || !arg) return;

  while((packet = chan_receiving(chan)))
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
    device->knock = malloc(sizeof(struct knock_struct));
    memset(device->knock,0,sizeof(struct knock_struct));
    return device;
  }
  return NULL;
}

// fills in next knock for this device only
tmesh_t tmesh_knock(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");

  // construct ping tx frame data, same for all the things
  if(k->mote->ping)
  {
    // copy in ping nonce
    memcpy(k->frame,k->nonce,8);
    // advance to the next future rx window
    while(mote_tx(k->mote)) mote_advance(k->mote,mote_next(k->mote));
    memcpy(k->frame+8,k->mote->nonce,8);
    // copy in hashname
    memcpy(k->frame+8+8,tm->mesh->id->bin,32);
    // random fill rest
    e3x_rand(k->frame+8+8+32,64-(8+8+32));

     // ciphertext frame after nonce
    chacha20(k->mote->secret,k->frame,k->frame+8,64-8);

    LOG("TX %s %s %s",k->mote->public?"public":"link",k->mote->pong?"pong":"ping",util_hex(k->frame,8,NULL));
    k->mote->priority++;

    return tm;
  }

  // fill in chunk tx
  int16_t size = util_chunks_size(k->mote->chunks);
  if(size < 0) return tm; // nothing to send, noop

  LOG("TX chunk frame size %d",size);

  // TODO, real header, term flag
  k->frame[0] = 0; // stub for now
  k->frame[1] = (uint8_t)size; // max 63
  memcpy(k->frame+2,util_chunks_frame(k->mote->chunks),size);
  // ciphertext full frame
  chacha20(k->mote->secret,k->nonce,k->frame,64);

  return tm;
}

// signal once a knock has been sent/received for this mote
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k, uint32_t ago)
{
  uint32_t drift;
  mote_t mlink = NULL;
  if(!tm || !k) return LOG("bad args");
  
  if(k->err)
  {
    LOG("knock error");
    return tm;
  }
  
  // calculate drift, difference between stop and done
  drift = k->done - k->stop;
  
  // clear any priority now
  k->mote->priority = 0;
  
  // tx just changes flags here
  if(k->tx)
  {
    // trust actual tx time to auto-correct for drift
    k->mote->at += drift;

    k->mote->sent++;
    LOG("tx done, total %d next at %lu",k->mote->sent,k->mote->at);

    // a sent pong sets this mote free
    if(k->mote->pong) mote_synced(k->mote);
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

    // if there is a link, validate
    if(k->mote->link)
    {
      // verify hashname match
      if(memcmp(k->frame+8+8,k->mote->link->id->bin,32) != 0)
      {
        if(k->mote->public)
        {
          LOG("removing (and currently leaking) stale cached link on public mote");
          k->mote->link = NULL;
        }else{
          return LOG("link ping hashname mismatch");
        }
      }
    }

    // create a link if none (public)
    if(k->mote->public && !k->mote->link)
    {
      if(memcmp(k->frame+8+8,tm->mesh->id->bin,32) == 0) return LOG("identity crisis");
      k->mote->link = link_get32(tm->mesh, k->frame+8+8);
      mlink = tmesh_link(tm, k->mote->com, k->mote->link);
      if(!mlink) return LOG("mote link failed");
      if(mlink->at == 0) LOG("new mote to %s",mlink->link->id->hashname);
    }

    // an incoming pong is sync, yay
    if(memcmp(k->nonce,k->frame,8) == 0)
    {
      LOG("incoming pong verified");
      k->mote->at += drift;
      mote_synced(k->mote);
      return tm;
    }

    LOG("incoming ping, preparing to send pong");

    // always sync wait to the bundled nonce for the next pong
    memcpy(k->mote->nonce,k->frame,8);
    
    // since there's no ordering w/ public pings, make sure we're inverted to the sender for the pong
    if(k->mote->public) k->mote->order = mote_tx(k->mote) ? 1 : 0;

    // fast forward to the ping's wait nonce
    k->mote->at = 0;
    uint8_t max = 100;
    while(--max && memcmp(k->frame+8, k->mote->nonce, 8) != 0) mote_advance(k->mote,mote_next(k->mote));
    // be safe
    if(!max || k->mote->at < ago)
    {
      LOG("failed to find wait nonce in time: %s",util_hex(k->frame+8,8,NULL));
      k->mote->at = mote_next(k->mote);
    }else{
      k->mote->at -= ago;
      k->mote->pong = 1;
      k->mote->priority += 2;
      LOG("looking for pong at %d with %s",k->mote->at,util_hex(k->mote->nonce,8,NULL));
    }

    // for safety force any referenced mote to wait till after next public window
    if(mlink) mlink->at = k->mote->at + k->mote->com->medium->max;

    return tm;
  }
  
  // received knock handling now, decipher frame
  chacha20(k->mote->secret,k->nonce,k->frame,64);
  
  // TODO check and validate frame[0] now

  // trust actual rx time to auto-correct for drift
  k->mote->at += drift;

  // received stats only after minimal validation
  k->mote->received++;
  
  LOG("rx done, total %d chunk len %d",k->mote->received,k->frame[1]);

  // process incoming chunk to link
  util_chunks_read(k->mote->chunks,k->frame+1,k->frame[1]+1);

  return tm;
}

// advance all windows forward this many microseconds, all knocks are relative to this call
uint32_t tmesh_process(tmesh_t tm, uint32_t us)
{
  cmnty_t com;
  mote_t mote;
  lob_t packet;
  struct knock_struct ktmp;
  knock_t knock;
  if(!tm || !us) return 0;

  for(com=tm->coms;com;com=com->next)
  {
    // block on any active knocks
    knock = radio_devices[com->medium->radio]->knock;
    if(knock->ready)
    {
      // if it's not done
      if(!knock->done)
      {
        knock->waiting += us;
        // update active knock reference time
        knock->start -= (knock->start < us) ? knock->start : us;
        knock->stop -= (knock->stop < us) ? knock->stop : us;
        LOG("active knock start %lu stop %lu busy %d waiting %lu",knock->start,knock->stop,knock->busy,knock->waiting);
        continue;
      }

      LOG("processing done knock %lu ago waited %lu",us - knock->done,knock->waiting);
      tmesh_knocked(tm, knock, us - knock->done);

      // add time we've been waiting for the unblocked motes below now
      us += knock->waiting;
      memset(knock,0,sizeof(struct knock_struct));
    }

    // walk the motes for processing and to find another knock
    memset(&ktmp,0,sizeof(struct knock_struct));
    for(mote=com->motes;mote;mote=mote->next)
    {
      // process any packets on this mote
      while((packet = util_chunks_receive(mote->chunks)))
        mesh_receive(tm->mesh, packet, com->pipe); // TODO associate mote for neighborhood

      // inform every mote of the time change, except ones already set to pong
      if(!mote->pong) mote_advance(mote,us);

      // TODO, optimize skipping tx knocks if nothing to send
      mote_knock(mote,&ktmp);

      // use the new one if preferred
      if(!knock->mote || tm->sort(knock,&ktmp) != knock)
      {
        memcpy(knock,&ktmp,sizeof(struct knock_struct));
      }
    }

    // no knock available
    if(!knock->mote) continue;

    // signal this knock is ready to roll
    knock->ready = 1;
    LOG("new %s knock to mote %s nonce %s",knock->tx?"TX":"RX",knock->mote->public?"anyone":knock->mote->link->id->hashname,util_hex(knock->nonce,8,NULL));

    // do the work to fill in the tx frame only once here
    if(knock->tx)
    {
      // bump tx start forward just a bit as a temp proxy for real drift calcs
      knock->start += 500;
      tmesh_knock(tm, knock);
    }
  }
  
  uint32_t best = 0xffffffff;
  int i;
  for(i=0;i<RADIOS_MAX;i++)
  {
    if(!radio_devices[i]) continue;
    knock = radio_devices[i]->knock;
    if(!knock->ready) continue;
    if(knock->done) continue;
    if(knock->busy)
    {
      if(knock->stop < best) best = knock->stop;
    }else{
      if(knock->start < best) best = knock->start;
    }
  }
  LOG("returning best next time of %lu",best);
  
  return best;
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

  return m;
}

// when is the next window start for this mote
uint32_t mote_next(mote_t m)
{
  uint32_t next;
  next = util_sys_long((unsigned long)*(m->nonce));
  // smaller for high z, using only high 4 bits of z
  next >>= (m->z >> 4);
  return next + m->com->medium->min + m->com->medium->max + m->at;
}

// least significant nonce bit sets direction
uint8_t mote_tx(mote_t m)
{
  return ((m->nonce[7] & 0b00000001) == m->order) ? 0 : 1;
}

// advance window by relative time
mote_t mote_advance(mote_t m, uint32_t us)
{
  if(!m || !us) return LOG("bad args");

  while(LOG("bttf us %lu > at %lu nonce %s",us,m->at,util_hex(m->nonce,8,NULL)) || m->at < us)
  {
    // rotate nonce by ciphering it
    chacha20(m->secret,m->nonce,m->nonce,8);
    m->at = mote_next(m);
  }
  
  // move relative forward
  m->at -= us;
  
  LOG("bttf done w/ at %lu",m->at);
  return m;
}

// next knock init
mote_t mote_knock(mote_t m, knock_t k)
{
  if(!m || !k) return LOG("bad args");

  k->mote = m;
  k->start = k->stop = 0;

  // set current non-ping channel
  if(!m->ping)
  {
    memset(m->chan,0,2);
    chacha20(m->secret,m->nonce,m->chan,2);
  }

  // set relative start/stop times
  k->start = m->at;
  k->stop = k->start + ((k->tx) ? m->com->medium->max : m->com->medium->min);

  // when receiving a ping, use wide window to catch more
  if(m->ping) k->stop = k->start + m->com->medium->max;

  // very remote case of overlow (70min minus max micros), just sets to max avail, slight inefficiency?
  if(k->stop < k->start) k->stop = 0xffffffff;

  // derive current channel
  k->chan = m->chan[1] % m->com->medium->chans;

  // direction
  k->tx = mote_tx(m);

  // cache nonce
  memcpy(k->nonce,m->nonce,8);

  return m;
}

// change mote to synchronized state (post-pong), initiates handshake over link mote
mote_t mote_synced(mote_t m)
{
  if(!m) return LOG("bad args");

  LOG("mote synchronized!");

  m->pong = 0;

  // handle public beacon motes special
  if(m->public)
  {
    // sync any cached link mote
    if(m->link)
    {
      mote_t lmote = tmesh_link(m->com->tm, m->com, m->link);
      lmote->at = m->at; // first link ping will trump next public ping
      m->link = NULL;
    }

    // TODO intelligent sleepy mode, not just skip some
    m->at += 1000*1000*5; // 5s

    return m;
  }
  
  // TODO, set up first sync timeout to reset!
  m->ping = 0;
  util_chunks_free(m->chunks);
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
  // any that finish before another starts
  if(a->stop < b->start) return a;
  if(b->stop < a->start) return b;
  // any higher priority state
  if(a->mote->priority > b->mote->priority) return a;
  if(b->mote->priority > a->mote->priority) return b;
  // any with link over without
  if(a->mote->link && !b->mote->link) return a;
  if(b->mote->link && !a->mote->link) return b;
  // firstie
  return (a->start < b->start) ? a : b;
}

