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
  for(m=c->active;m;m=m->next) if(m->link == link)
  {
    util_chunks_send(m->chunks, packet);
    LOG("delivering %d to mote, %d",lob_len(packet),util_chunks_size(m->chunks));
    return;
  }

  LOG("no link in community");
  lob_free(packet);
}

static cmnty_t cmnty_free(cmnty_t com)
{
  if(!com) return NULL;
  radio_t radio = radio_devices[com->medium->radio];
  if(radio->free) radio->free(radio, com->medium);
  free(com->medium);
  
  // do not free c->name, it's part of c->pipe
  // TODO free motes
//  pipe_free(c->pipe); path may be in mesh->paths 

  free(com);
  return NULL;
}

// create a new blank community
static cmnty_t cmnty_new(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c;
  uint8_t bin[5];
  if(!tm || !name || !medium || strlen(medium) != 8) return LOG("bad args");
  if(base32_decode(medium,0,bin,5) != 5) return LOG("bad medium encoding: %s",medium);

  if(!(c = malloc(sizeof (struct cmnty_struct)))) return LOG("OOM");
  memset(c,0,sizeof (struct cmnty_struct));
  c->tm = tm;

  if(!(c->medium = malloc(sizeof (struct medium_struct)))) return cmnty_free(c);
  memset(c->medium,0,sizeof (struct medium_struct));

  // try to init the medium
  for(uint8_t i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if(radio_devices[i]->init(radio_devices[i],c->medium))
    {
      c->medium->radio = i;
      break;
    }
  }
  if(!c->medium->radio) LOG("unsupported medium %s",medium);
  if(!c->medium->radio) return cmnty_free(c);

  // set up pipe
  if(!(c->pipe = pipe_new("tmesh"))) return cmnty_free(c);
  c->pipe->arg = c;
  c->name = c->pipe->id = strdup(name);
  c->pipe->send = cmnty_send;

  // make official
  c->next = tm->coms;
  tm->coms = c;

  return c;
}


//////////////////
// public methods

// join a new private/public community
cmnty_t tmesh_join(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c = cmnty_new(tm,medium,name);
  if(!c) return LOG("bad args");

  // generate a beacon and detection mote
  if(!(c->passive = mote_new(c->medium, NULL))) return cmnty_free(c);
  c->passive->beacon = 1;
  if(!(c->passive->next = mote_new(c->medium, NULL))) return cmnty_free(c);
  c->passive->next->detection = 1;

  if(c->public)
  {
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
mote_t tmesh_link(tmesh_t tm, cmnty_t com, link_t link)
{
  mote_t m;
  if(!tm || !com || !link) return LOG("bad args");

  // check list of motes, add if not there
  for(m=com->active;m;m = m->next) if(m->link == link) return m;

  if(!(m = mote_new(com->medium, link))) return LOG("OOM");
  m->next = com->active;
  com->active = m;
  
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
  tm->epoch = 1;
  e3x_rand(tm->seed,4);
  
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
  if(k->ping)
  {
    // copy in ping nonce
    memcpy(k->frame,k->nonce,8);

    // advance to the next future rx window
    while(mote_tx(k->mote)) mote_advance(k->mote);
    memcpy(k->frame+8,k->mote->nonce,8);

    // copy in hashname
    memcpy(k->frame+8+8,hashname_bin(tm->mesh->id),32);

    // copy in our reset seed
    memcpy(k->frame+8+8+32,tm->seed,4);

    // random fill rest
    e3x_rand(k->frame+8+8+32+4,64-(8+8+32+4));

     // ciphertext frame after nonce
    chacha20(k->mote->secret,k->frame,k->frame+8,64-8);

    LOG("TX %s %s %s",k->mote->public?"public":"link",k->mote->pong?"pong":"ping",util_hex(k->frame,8,NULL));

    return tm;
  }

  // fill in chunk tx
  if(util_chunks_size(k->mote->chunks) <= 0)
  {
    // nothing to send, noop
    k->mote->txz++;
    return tm;
  }

  uint8_t size = util_chunks_size(k->mote->chunks);
  if(size == 62 && util_chunks_peek(k->mote->chunks) == 0) size++; // flag is terminated even tho full

  // TODO, real header, term flag
  k->frame[0] = 0; // stub for now
  k->frame[1] = size; // max 63
  memcpy(k->frame+2,util_chunks_frame(k->mote->chunks),size);

  // ciphertext full frame
  LOG("TX chunk frame %d: %s",size,util_hex(k->frame,64,NULL));
  chacha20(k->mote->secret,k->nonce,k->frame,64);

  return tm;
}

// signal once a knock has been sent/received for this mote
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k)
{
  mote_t mlink = NULL;
  if(!tm || !k) return LOG("bad args");
  if(!k->ready) return LOG("knock wasn't ready");
  
  // clear some flags straight away
  k->mote->priority = 0;
  k->ready = 0;
  
  if(k->err)
  {
    if(!k->tx) k->mote->rxz++; // count missed rx knocks
    LOG("knock error");
    k->mote->pong = 0; // always clear pong state
    return tm;
  }
  
  // tx just updates state things here
  if(k->tx)
  {
    k->mote->txz = 0;
    LOG("tx done, next at %lu",k->mote->at);
    
    if(k->mote->pong)
    {
      // sent pong shifts start time to when actually done
      k->mote->at = k->done;
      // a sent pong sets this mote free
      mote_synced(k->mote);
      k->mote->pong = 0;
    }else if(k->ping){
      // sent ping will always rebase time to when tx was actually completed
      k->mote->at = k->done;
      // restore the sent nonce
      memcpy(k->mote->nonce,k->nonce,8);
      // re-advance to the next future rx window from adjusted time
      while(mote_tx(k->mote)) mote_advance(k->mote);
      k->mote->pong = 1;
      k->mote->priority++;
      LOG("scheduled pong rx at %d with %s",k->mote->at,util_hex(k->mote->nonce,8,NULL));
    }else{
      LOG("chunk next");
      // advance chunks if any
      util_chunks_next(k->mote->chunks);
      // we don't use zeros so dump em
      if(util_chunks_size(k->mote->chunks) == 0) util_chunks_next(k->mote->chunks);
    }

    return tm;
  }

  // rx knock handling now
  
  // update last seen rssi for all
  k->mote->last = k->rssi;

  // ooh, got a ping eh?
  if(k->ping)
  {
    LOG("PING RX %s RSSI %d",util_hex(k->frame,8,NULL),k->rssi);

    // first decipher frame using given nonce
    chacha20(k->mote->secret,k->frame,k->frame+8,64-8);

    // if there is a link, validate
    if(k->mote->link)
    {
      // verify hashname match
      if(memcmp(k->frame+8+8,hashname_bin(k->mote->link->id),32) != 0)
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

    // rebase all ping starts to when actually done receiving
    k->mote->at = k->done;

    // create a link if none (public)
    if(k->mote->public && !k->mote->link)
    {
      hashname_t id = hashname_vbin(k->frame+8+8);
      if(hashname_cmp(id,tm->mesh->id) == 0) return LOG("identity crisis");
      link_t link = link_get(tm->mesh, id);
      mlink = tmesh_link(tm, k->mote->com, link);
      if(!mlink) return LOG("mote link failed");
      if(memcmp(mlink->seed,k->frame+8+8+32,4) == 0) return LOG("ignoring public ping for an active mote");
      LOG("new/reset mote to %s",hashname_short(mlink->link->id));
      mlink->at = 0xffffffff; // disabled, as mote_synced of public will update it
      k->mote->link = link; // cache for synced
    }

    // an incoming pong is sync signal, much celebration
    if(memcmp(k->nonce,k->frame,8) == 0)
    {
      LOG("incoming pong is to legit to quit");
      // copy in the given bundled sync nonce and reset seed
      memcpy(k->mote->nonce,k->frame+8,8);
      memcpy(k->mote->seed,k->frame+8+8+32,4);
      mote_synced(k->mote);
      return tm;
    }

    LOG("incoming ping, preparing to send pong");

    // always sync seed and wait to the bundled nonce for the next pong
    memcpy(k->mote->nonce,k->frame,8);
    memcpy(k->mote->seed,k->frame+8+8+32,4);
    
    // since there's no ordering w/ public pings, make sure we're inverted to the sender for the pong
    if(k->mote->public) k->mote->order = mote_tx(k->mote) ? 1 : 0;

    // fast forward to next tx
    while(!mote_tx(k->mote)) mote_advance(k->mote);

    if(memcmp(k->frame+8, k->mote->nonce, 8) != 0)
    {
      LOG("invalid nonce received: %s",util_hex(k->frame+8,8,NULL));
    }else{
      k->mote->pong = 1;
      k->mote->priority += 2;
      LOG("scheduled pong tx at %d with %s",k->mote->at,util_hex(k->mote->nonce,8,NULL));
    }

    return tm;
  }
  
  // received knock handling now, decipher frame
  chacha20(k->mote->secret,k->nonce,k->frame,64);
  LOG("RX chunk frame: %s",util_hex(k->frame,64,NULL));
  
  // TODO check and validate frame[0] now

  // if avg time is provided, calculate a drift based on when this rx was done
  uint32_t avg = k->medium->avg;
  uint32_t took = k->done - k->start;
  if(avg && took > avg)
  {
    LOG("adjusting for drift by %lu, took %lu avg %lu",took - avg,took,avg);
    k->mote->at += (took - avg);
  }
  
  uint8_t size = k->frame[1];
  if(size > 63) return LOG("invalid chunk frame, too large: %d",size);

  // received stats only after minimal validation
  k->mote->rxz = 0;
  if(k->rssi < k->mote->best) k->mote->best = k->rssi;
  if(k->rssi > k->mote->worst) k->mote->worst = k->rssi;
  
  LOG("rx done, chunk len %d rssi %d/%d/%d",size,k->mote->last,k->mote->best,k->mote->worst);

  // process incoming chunk to link
  util_chunks_chunk(k->mote->chunks,k->frame+2,size);
  // if terminated
  if(size < 62 || size == 63) util_chunks_chunk(k->mote->chunks,NULL,0);

  return tm;
}

// process everything based on current cycle count, returns success
tmesh_t tmesh_process(tmesh_t tm, uint32_t at, uint32_t rebase)
{
  cmnty_t com;
  mote_t mote;
  lob_t packet;
  struct knock_struct k1, k2;
  knock_t knock;
  if(!tm || !at) return NULL;
  
  LOG("processing for %s at %lu",hashname_short(tm->mesh->id),at);

  // we are looking for the next knock anywhere
  for(com=tm->coms;com;com=com->next)
  {
    radio_t radio = radio_devices[com->medium->radio];
    knock = radio->knock;
    
    // clean slate
    memset(&k1,0,sizeof(struct knock_struct));
    if(!knock->ready) memset(knock,0,sizeof(struct knock_struct));

    // walk the local motes
    for(mote=com->motes;mote;mote=mote->next)
    {
      // first rebase cycle count if requested
      if(rebase) mote->at -= rebase;

      // move ahead window(s)
      while(mote->at < at) mote_advance(mote);
      LOG("mote at %lu %s %s",mote->at,util_hex(mote->nonce,8,NULL),mote->public?"public":hashname_short(mote->link->id));

      // already have one active
      if(knock->ready) continue;

      // peek at the next knock details
      memset(&k2,0,sizeof(struct knock_struct));
      mote_knock(mote,&k2);
      
      // boost non-public every round
      if(!mote->public) mote->priority++;

      // use the new one if preferred
      if(!k1.mote || tm->sort(&k1,&k2) != &k1)
      {
        memcpy(&k1,&k2,sizeof(struct knock_struct));
      }
    }

    // no new knock available
    if(!k1.mote) continue;

    // signal this knock is ready to roll
    memcpy(knock,&k1,sizeof(struct knock_struct));
    knock->ready = 1;
    LOG("new %s knock at %lu nonce %s to mote %s",knock->tx?"TX":"RX",knock->start,util_hex(knock->nonce,8,NULL),knock->mote->public?"anyone":hashname_short(knock->mote->link->id));

    // do the work to fill in the tx frame only once here
    if(knock->tx) tmesh_knock(tm, knock);
    
    // signal driver
    if(radio->ready && !radio->ready(radio, tm, knock))
    {
      LOG("radio ready driver failed, cancelling knock");
      memset(knock,0,sizeof(struct knock_struct));
      continue;
    }
  }

  // now do any heavier processing work decoupled
  for(com=tm->coms;com;com=com->next)
  {
    for(mote=com->motes;mote;mote=mote->next)
    {
      // process any packets on this mote
      while((packet = util_chunks_receive(mote->chunks)))
      {
        LOG("mote %.6s pkt %s",mote->public?"public":hashname_short(mote->link->id),lob_json(packet));
        // TODO make this more of a protocol, not a hack
        if(lob_get(packet,"1a"))
        {
          lob_t keys = lob_new();
          lob_set_base32(keys,"1a",packet->body,packet->body_len);
          lob_set_raw(packet,"keys",0,(char*)keys->head,keys->head_len);
          lob_free(keys);
        }
        // TODO associate mote for neighborhood
        mesh_receive(tm->mesh, packet, com->pipe);
      }
    }
  }
  
  // overall telehash background processing now based on seconds
  if(rebase) tm->last -= rebase;
  tm->cycles += (at - tm->last);
  tm->last = at;
  while(tm->cycles > 32768)
  {
    tm->epoch++;
    tm->cycles -= 32768;
  }
  LOG("mesh process epoch %lu",tm->epoch);
  mesh_process(tm->mesh,tm->epoch);

  return tm;
}

mote_t mote_new(medium_t medium, link_t link)
{
  uint8_t i;
  tmesh_t tm;
  mote_t m;
  
  if(!medium || !medium->com || !medium->com->tm) return LOG("internal error");
  tm = medium->com->tm;

  if(!(m = malloc(sizeof(struct mote_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct mote_struct));

  // determine ordering based on hashname compare
  if(link)
  {
    if(!(m->chunks = util_chunks_new(63))) return mote_free(m);
    m->link = link;
    uint8_t *bin1 = hashname_bin(link->id);
    uint8_t *bin2 = hashname_bin(link->mesh->id);
    for(i=0;i<32;i++)
    {
      if(bin1[i] == bin2[i]) continue;
      m->order = (bin1[i] > bin2[i]) ? 1 : 0;
      break;
    }
    // TODO set up link down event handler to remove this mote
  }
  
  m->at = tm->last;
  m->medium = medium;

  return mote_reset(m);
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

  // reset to defaults
  m->z = m->medium->z;
  m->txz = m->rxz = 0;
  m->last = m->best = m->worst = 0;
  memset(m->seed,0,4);

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
      a = hashname_bin(m->link->id);
      b = hashname_bin(m->link->mesh->id);
    }else{
      b = hashname_bin(m->link->id);
      a = hashname_bin(m->link->mesh->id);
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
  
  return m;
}

// least significant nonce bit sets direction
uint8_t mote_tx(mote_t m)
{
  return ((m->nonce[7] & 0b00000001) == m->order) ? 0 : 1;
}

// advance window by one
mote_t mote_advance(mote_t m)
{
  // rotate nonce by ciphering it
  chacha20(m->secret,m->nonce,m->nonce,8);

  uint32_t next = util_sys_long((unsigned long)*(m->nonce));

  // smaller for high z, using only high 4 bits of z
  next >>= (m->z >> 4) + m->com->medium->zshift;

  m->at += next + m->com->medium->min + m->com->medium->max;
  
  LOG("advanced to nonce %s at %lu next %lu",util_hex(m->nonce,8,NULL),m->at,next);

  return m;
}

// next knock init
mote_t mote_knock(mote_t m, knock_t k)
{
  if(!m || !k) return LOG("bad args");

  k->mote = m;
  k->medium = m->com->medium; // shortcut
  k->start = k->stop = 0;
  k->ping = k->pong = 0;

  // set current active channel
  if(m->link)
  {
    memset(m->chan,0,2);
    chacha20(m->secret,m->nonce,m->chan,2);
  }

  // direction
  k->tx = mote_tx(m);

  // set ping/pong mode flags if passive
  if(m->beacon)
  {
    k->ping = k->tx;
    k->pong = !k->tx
  }else if(m->detection){
    k->ping = !k->tx;
    k->pong = k->tx
  }

  // set relative start/stop times
  k->start = m->at;
  // listen window is larger for a beacon
  k->stop = k->start + (k->tx && m->beacon) ? k->medium->max : k->medium->min);

  // derive current channel
  k->chan = m->chan[1] % k->medium->chans;

  // cache nonce
  memcpy(k->nonce,m->nonce,8);

  // deprioritize mote if nothing to tx (TODO, inverse and prioritize?)
  if(m->link && k->tx && util_chunks_size(m->chunks) <= 0) m->priority = 0;

  return m;
}

// change mote to synchronized state (post-pong), initiates handshake over link mote
mote_t mote_synced(mote_t m)
{
  if(!m) return LOG("bad args");

  LOG("mote synchronized! %s",m->public?"public":hashname_short(m->link->id));
  m->ping = m->pong = 0;

  // handle public beacon motes special
  if(m->public)
  {
    // sync any cached link mote to same time/nonce seed
    if(m->link)
    {
      mote_t lmote = tmesh_link(m->com->tm, m->com, m->link);
      lmote->at = m->at;
      lmote->last = m->last;
      memcpy(lmote->nonce,m->nonce,8);
      mote_advance(lmote); // step forward a window
      m->link = NULL;
      LOG("public-sync'd link to %s at %lu from public at %lu",util_hex(lmote->nonce,8,NULL),lmote->at,m->at);
      mote_synced(lmote);
    }

    // TODO intelligent ping re-schedule, not just skip some and randomize
    m->at += 32768*15; // 15s
    e3x_rand(m->nonce,8);
    m->ping = 1;

    return m;
  }
  
  // TODO, set up first sync timeout to reset!
  util_chunks_free(m->chunks);
  m->chunks = util_chunks_new(63);
  m->chunks->blocking = 0;
  // establish the pipe path
  link_pipe(m->link,m->com->pipe);
  // if public and no keys, send discovery
  if(m->com->tm->pubim && !m->link->x)
  {
    LOG("sending bare discovery %s",lob_json(m->com->tm->pubim));
    pipe_send(m->com->pipe, lob_copy(m->com->tm->pubim), m->link);
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

