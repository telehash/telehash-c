#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

#define MORTY(mote) LOG("%s %s mote %u %s nonce %s at %u",mote->public?"pub":"pri",mote->beacon?"bekn":"link",mote->priority,hashname_short(mote->link?mote->link->id:mote->beacon),util_hex(mote->nonce,8,NULL),mote->at);

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
  for(m=c->links;m;m=m->next) if(m->link == link)
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
  memcpy(c->medium->bin,bin,5);
  c->medium->com = c;

  // try to init the medium
  uint8_t i=0;
  for(;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if(radio_devices[i]->init(radio_devices[i],c->medium))
    {
      c->medium->radio = i;
      break;
    }
  }
  if(i == RADIOS_MAX) LOG("unsupported medium %s",medium);
  if(i == RADIOS_MAX) return cmnty_free(c);

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
  if(!c || !name) return LOG("bad args");

  if(strncmp(name,"Public",6) == 0)
  {
    LOG("joining public community %s on medium %s",name,medium);

    // add a public beacon mote using zeros hashname
    uint8_t zeros[32] = {0};
    if(!(c->beacons = mote_new(c->medium, hashname_vbin(zeros)))) return cmnty_free(c);
    c->beacons->beacon = hashname_dup(hashname_vbin(zeros));;
    c->beacons->public = 1; // convenience flag for altered logic
    mote_reset(c->beacons);

    // one-time generate public intermediate keys packet
    if(!tm->pubim) tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
    
  }else{
    LOG("joining private community %s on medium %s",name,medium);
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

// add a link known to be in this community
mote_t tmesh_link(tmesh_t tm, cmnty_t com, link_t link)
{
  mote_t m;
  if(!tm || !com || !link) return LOG("bad args");

  // check list of motes, add if not there
  for(m=com->links;m;m = m->next) if(m->link == link) return m;

  // make sure there's a beacon mote also for syncing
  if(!(tmesh_seek(tm, com, link->id))) return LOG("OOM");

  if(!(m = mote_new(com->medium, link->id))) return LOG("OOM");
  m->link = link;
  m->next = com->links;
  com->links = m;

  // TODO set up link down event handler to remove this mote
  
  return mote_reset(m);
}

// start looking for this hashname in this community, will link once found
mote_t tmesh_seek(tmesh_t tm, cmnty_t com, hashname_t id)
{
  mote_t m = NULL;
  if(!tm || !com || !id) return LOG("bad args");

  // check list of beacons, add if not there
  for(m=com->beacons;m;m = m->next) if(hashname_cmp(m->beacon,id) == 0) return m;

  if(!(m = mote_new(com->medium, id))) return LOG("OOM");
  m->beacon = hashname_dup(id);
  m->next = com->beacons;
  com->beacons = m;
  
  // TODO gc unused beacons

  return mote_reset(m);
}

// if there's a mote for this link, return it
mote_t tmesh_mote(tmesh_t tm, link_t link)
{
  if(!tm || !link) return LOG("bad args");
  cmnty_t c;
  for(c=tm->coms;c;c=c->next)
  {
    mote_t m;
    for(m=c->links;m;m = m->next) if(m->link == link) return m;
  }
  return LOG("no mote found for link %s",hashname_short(link->id));
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

// fills in next tx knock
tmesh_t tmesh_knock(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");

  MORTY(k->mote);

  // send data frames if any
  if(k->mote->chunks)
  {
    int16_t size = util_chunks_size(k->mote->chunks);
      // nothing to send, noop
    if(size <= 0)
    {
      k->mote->txz++;
      return tm;
    }

    // flag is terminated even tho full
    if(size == 62 && util_chunks_peek(k->mote->chunks) == 0) size++;

    // TODO, real header, term flag
    k->frame[0] = 0; // stub for now
    k->frame[1] = size; // max 63
    memcpy(k->frame+2,util_chunks_frame(k->mote->chunks),size);

    // ciphertext full frame
    LOG("TX chunk frame %d: %s",size,util_hex(k->frame,64,NULL));
    chacha20(k->mote->secret,k->nonce,k->frame,64);
    return tm;
  }

  // construct beacon tx frame data, same for ping or pong

  // copy in ping nonce
  memcpy(k->frame,k->nonce,8);

  // advance to the next future rx window to get that nonce
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

  LOG("TX %s %s",k->mote->ack?"ack":"seek",util_hex(k->frame,8,NULL));

  return tm;
}

// signal once a knock has been sent/received for this mote
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");
  if(!k->ready) return LOG("knock wasn't ready");
  
  MORTY(k->mote);

  // clear some flags straight away
  k->ready = 0;
  
  if(k->err)
  {
    if(!k->tx) k->mote->rxz++; // count missed rx knocks
    k->mote->ack = 0; // always clear ack state
    LOG("knock error");
    return tm;
  }
  
  // tx just updates state things here
  if(k->tx)
  {
    k->mote->txz = 0; // clear skipped tx's
    LOG("tx done, next at %lu",k->mote->at);
    
    // did we send a data frame?
    if(k->mote->chunks)
    {
      LOG("chunk sent %d next %d",util_chunks_size(k->mote->chunks),util_chunks_peek(k->mote->chunks));
      // advance chunks
      util_chunks_next(k->mote->chunks);
      // we don't use flush zeros so dump em
      if(util_chunks_size(k->mote->chunks) == 0) util_chunks_next(k->mote->chunks);
      return tm;
    }
    
    // beacons always shift start time to when actually done for sync
    k->mote->at = k->done;

    // a sent ack starts the handshake
    if(k->mote->ack)
    {
      mote_handshake(k->mote);
      k->mote->ack = 0;
      return tm;
    }
    
    // a sent beacon waits for an ack

    // restore the sent nonce
    memcpy(k->mote->nonce,k->nonce,8);
    // re-advance to the next future rx window from adjusted time
    while(mote_tx(k->mote)) mote_advance(k->mote);
    k->mote->ack = 1;
    k->mote->priority = 2;
    LOG("scheduled beacon rx ack at %d with %s",k->mote->at,util_hex(k->mote->nonce,8,NULL));

    return tm;
  }
  
  // rx handling, update last seen rssi for all
  k->mote->last = k->rssi;

  // rx data frame
  if(k->mote->chunks)
  {
    chacha20(k->mote->secret,k->nonce,k->frame,64);
    LOG("RX data frame: %s",util_hex(k->frame,64,NULL));
  
    // TODO check and validate frame[0] now

    // if avg time is provided, calculate a drift based on when this rx was done
    /* TODO
    uint32_t avg = k->mote->medium->avg;
    uint32_t took = k->done - k->start;
    if(avg && took > avg)
    {
      LOG("adjusting for drift by %lu, took %lu avg %lu",took - avg,took,avg);
      k->mote->at += (took - avg);
    }
    */

    uint8_t size = k->frame[1];
    if(size > 63) return LOG("invalid chunk frame, too large: %d",size);

    // received stats only after minimal validation
    k->mote->rxz = 0;
    if(k->rssi < k->mote->best) k->mote->best = k->rssi;
    if(k->rssi > k->mote->worst) k->mote->worst = k->rssi;
  
    LOG("rx data received, chunk len %d rssi %d/%d/%d",size,k->mote->last,k->mote->best,k->mote->worst);

    // process incoming chunk to link
    util_chunks_chunk(k->mote->chunks,k->frame+2,size);

    // if terminated
    if(size < 62 || size == 63) util_chunks_chunk(k->mote->chunks,NULL,0);
    
    // if link mote, done
    if(k->mote->link) return tm;

    // for beacon motes we process only handshakes here and create/sync link if good
    lob_t packet;
    while((packet = util_chunks_receive(k->mote->chunks)))
    {
      link_t link = NULL;
      MORTY(k->mote);
      LOG("beacon packet %s",lob_json(packet));
      
      // only receive raw handshakes
      if(packet->head_len == 1) link = mesh_receive(tm->mesh, packet, NULL);
      else if(tm->pubim && lob_get(packet,"1a"))
      {
        // if public, try new link
        lob_t keys = lob_new();
        lob_set_base32(keys,"1a",packet->body,packet->body_len);
        link = link_keys(tm->mesh, keys);
        lob_free(keys);
        lob_free(packet);
      }
      
      if(link)
      {
        LOG("established link");
        mote_t linked = tmesh_link(tm, k->mote->medium->com, link);
        mote_sync(k->mote,linked);
        link_pipe(link,k->mote->medium->com->pipe); // establish default pipe for it
        lob_free(link_resync(link));
      }else{
        LOG("no link found");
      }
    }

    return tm;
  }

  // rx beacon handling now
  
  LOG("RX beacon rssi %d frame %s",k->rssi,util_hex(k->frame,64,NULL));

  // first decipher the frame using the bundled nonce
  chacha20(k->mote->secret,k->frame,k->frame+8,64-8);

  // rebase all beacon starts to when actually done receiving for sync
  k->mote->at = k->done;
  
  // get the private beacon to this hashname
  hashname_t id = hashname_vbin(k->frame+8+8);
  if(!id) return LOG("OOM");
  if(hashname_cmp(id,tm->mesh->id) == 0) return LOG("identity crisis");

  // public beacon mote will spawn/sync private beacons
  if(k->mote->public)
  {
    mote_t beacon = tmesh_seek(tm, k->mote->medium->com, id);
    if(!beacon) return LOG("OOM");

    // check the seed to see if there was a reset
    if(memcmp(beacon->seed,k->frame+8+8+32,4) == 0) return LOG("skipping public beacon for an active mote");
    
    // a nonce that matches is a reply, sync the beacon and start handshake
    if(memcmp(k->mote->nonce,k->frame,8) == 0)
    {
      LOG("incoming public beacon, u can't touch this");
      mote_reset(beacon);
      beacon->at = k->mote->at;
      memcpy(beacon->nonce,k->mote->nonce,8);
      mote_advance(beacon); // one window past so no conflict
      mote_handshake(beacon);
      return tm;
    }
    
    // sync
    memcpy(k->mote->nonce,k->frame,8);
    LOG("new incoming public beacon from %s %s",hashname_short(id),util_hex(k->mote->nonce,8,NULL));

    // since there's no ordering w/ public beacons, make sure we're inverted to the sender for the ack
    k->mote->order = mote_tx(k->mote) ? 1 : 0;

    // fast forward to the ack
    while(!mote_tx(k->mote)) mote_advance(k->mote);

    // reset/sync the private beacon to start handshaking after that
    mote_reset(beacon);
    beacon->at = k->mote->at;
    memcpy(beacon->nonce,k->mote->nonce,8);
    mote_advance(beacon); // one window past so no conflict
    mote_handshake(beacon);
    
    return tm;
  }

  // given hashname must match this private beacon
  if(hashname_cmp(k->mote->beacon,id) != 0) return LOG("private beacon id mismatch: %s",hashname_char(id));
  
  // check the seed to see if there was a reset
  if(memcmp(k->mote->seed,k->frame+8+8+32,4) == 0) return LOG("skipping private beacon for a known mote");

  // an incoming matching nonce is sync signal, much celebration
  if(memcmp(k->mote->nonce,k->frame,8) == 0)
  {
    LOG("incoming private beacon is to legit to quit");
    // copy in the given bundled sync nonce and cache seed
    memcpy(k->mote->nonce,k->frame+8,8);
    memcpy(k->mote->seed,k->frame+8+8+32,4);
    mote_handshake(k->mote);
    return tm;
  }

  LOG("new incoming private beacon, sync and ack");

  // sync to the beacon nonce and fast forward to the next tx
  memcpy(k->mote->nonce,k->frame,8);
  while(!mote_tx(k->mote)) mote_advance(k->mote);
  
  // they must match for validation
  if(memcmp(k->frame+8, k->mote->nonce, 8) != 0) return LOG("invalid nonce received: %s",util_hex(k->frame+8,8,NULL));

  k->mote->ack = 1;
  k->mote->priority = 3;
  LOG("scheduled beacon ack tx at %d with %s",k->mote->at,util_hex(k->mote->nonce,8,NULL));

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

    // walk all the motes for next best knock
    mote_t next = NULL;
    for(mote=com->beacons;mote;mote=next)
    {
      // switch to link motes list after beacons
      next = mote->next;
      if(!next && !mote->link) next = com->links;

      // first rebase cycle count if requested
      if(rebase) mote->at -= rebase;

      // already have one active, noop
      if(knock->ready) continue;

      // move ahead window(s)
      while(mote->at < at) mote_advance(mote);
      MORTY(mote);

      // peek at the next knock details
      memset(&k2,0,sizeof(struct knock_struct));
      mote_knock(mote,&k2);
      
      // set priority when there's data to send
      if(k2.tx && mote->chunks)
      {
        if(util_chunks_size(mote->chunks) > 0) mote->priority++;
        else mote->priority = 0;
      }

      // use the new one if preferred
      if(!k1.mote || tm->sort(&k1,&k2) != &k1)
      {
        LOG("electing");MORTY(k2.mote);
        memcpy(&k1,&k2,sizeof(struct knock_struct));
      }
    }

    // no new knock available
    if(!k1.mote) continue;

    // signal this knock is ready to roll
    memcpy(knock,&k1,sizeof(struct knock_struct));
    knock->ready = 1;
    LOG("new %s knock at %lu nonce %s",knock->tx?"TX":"RX",knock->start,util_hex(knock->nonce,8,NULL));
    MORTY(knock->mote);

    // do the work to fill in the tx frame only once here
    if(knock->tx) tmesh_knock(tm, knock);
    
    // signal driver
    if(radio->ready && !radio->ready(radio, knock->mote->medium, knock))
    {
      LOG("radio ready driver failed, cancelling knock");
      memset(knock,0,sizeof(struct knock_struct));
      continue;
    }
  }

  // now do any heavier processing work decoupled
  for(com=tm->coms;com;com=com->next)
  {
    // TODO split out beacon motes for timeouts!
    for(mote=com->links;mote;mote=mote->next)
    {
      // process any packets on this mote
      while((packet = util_chunks_receive(mote->chunks)))
      {
        MORTY(mote);
        LOG("pkt %s",lob_json(packet));
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

mote_t mote_new(medium_t medium, hashname_t id)
{
  uint8_t i;
  tmesh_t tm;
  mote_t m;
  
  if(!medium || !medium->com || !medium->com->tm || !id) return LOG("internal error");
  tm = medium->com->tm;

  if(!(m = malloc(sizeof(struct mote_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct mote_struct));
  m->at = tm->last;
  m->medium = medium;

  // determine ordering based on hashname compare
  uint8_t *bin1 = hashname_bin(id);
  uint8_t *bin2 = hashname_bin(tm->mesh->id);
  for(i=0;i<32;i++)
  {
    if(bin1[i] == bin2[i]) continue;
    m->order = (bin1[i] > bin2[i]) ? 1 : 0;
    break;
  }
  
  return m;
}

mote_t mote_free(mote_t m)
{
  if(!m) return NULL;
  hashname_free(m->beacon);
  util_chunks_free(m->chunks);
  free(m);
  return NULL;
}

mote_t mote_reset(mote_t m)
{
  uint8_t *a, *b, roll[64];
  if(!m || !m->medium) return LOG("bad args");
  tmesh_t tm = m->medium->com->tm;
  
  // reset to defaults
  m->z = m->medium->z;
  m->ack = 0;
  m->txz = m->rxz = 0;
  m->last = m->best = m->worst = 0;
  memset(m->seed,0,4);
  m->chunks = util_chunks_free(m->chunks);
  if(m->link)
  {
    m->chunks = util_chunks_new(63);
    if(m->chunks) m->chunks->blocking = 0;
    m->priority = 0; // not until data
  }else{
    m->priority = 1; // beacon defaults higher
  }

  // TODO detach pipe from link?

  // generate mote-specific secret, roll up community first
  e3x_hash(m->medium->bin,5,roll);
  e3x_hash((uint8_t*)(m->medium->com->name),strlen(m->medium->com->name),roll+32);
  e3x_hash(roll,64,m->secret);

  if(m->public)
  {
    // public uses single shared hashname
    a = b = hashname_bin(m->beacon);
  }else{
    // add both hashnames in order
    hashname_t id = (m->link) ? m->link->id : m->beacon;
    if(m->order)
    {
      a = hashname_bin(id);
      b = hashname_bin(tm->mesh->id);
    }else{
      b = hashname_bin(id);
      a = hashname_bin(tm->mesh->id);
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
  
  MORTY(m);

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
  next >>= (m->z >> 4) + m->medium->zshift;

  m->at += next + m->medium->min + m->medium->max;
  
  LOG("advanced to nonce %s at %u next %u",util_hex(m->nonce,8,NULL),m->at,next);

  return m;
}

// next knock init
mote_t mote_knock(mote_t m, knock_t k)
{
  if(!m || !k) return LOG("bad args");

  k->mote = m;
  k->start = k->stop = 0;

  // set current active channel
  if(m->link)
  {
    memset(m->chan,0,2);
    chacha20(m->secret,m->nonce,m->chan,2);
  }

  // direction
  k->tx = mote_tx(m);

  // set relative start/stop times
  k->start = m->at;
  // listen window is larger for a beacon
  k->stop = k->start + ((!k->tx && m->beacon) ? m->medium->max : m->medium->min);

  // derive current channel
  k->chan = m->chan[1] % m->medium->chans;

  // cache nonce
  memcpy(k->nonce,m->nonce,8);

  return m;
}

// synchronizes two motes
mote_t mote_sync(mote_t source, mote_t target)
{
  if(!source || !target) return LOG("bad args");
  target->at = source->at;
  memcpy(target->nonce,source->nonce,8);
  return target;
}

// initiates handshake over beacon mote
mote_t mote_handshake(mote_t m)
{
  if(!m) return LOG("bad args");

  MORTY(m);

  // TODO, set up first sync timeout to reset!
  util_chunks_free(m->chunks);
  if(!(m->chunks = util_chunks_new(63))) return LOG("OOM");
  m->chunks->blocking = 0;
  // if public and no keys, send discovery
  if(m->medium->com->tm->pubim && (!m->link || !m->link->x))
  {
    LOG("sending bare discovery %s",lob_json(m->medium->com->tm->pubim));
    util_chunks_send(m->chunks, lob_copy(m->medium->com->tm->pubim));
    return m;
  }else{
    // send handshake
    util_chunks_send(m->chunks, link_resync(m->link));
  }

  return m;
}

knock_t knock_sooner(knock_t a, knock_t b)
{
  LOG("sort a%u %u/%u b%u %u/%u",a->mote->priority,a->start,a->stop,b->mote->priority,b->start,b->stop);
  // any that finish before another starts
  if(a->stop < b->start) return a;
  if(b->stop < a->start) return b;
  // any higher priority state
  if(a->mote->priority > b->mote->priority) return a;
  if(b->mote->priority > a->mote->priority) return b;
  // firstie
  return (a->start < b->start) ? a : b;
}

