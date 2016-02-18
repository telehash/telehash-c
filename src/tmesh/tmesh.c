#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

// util debug dump of mote state
#define MORTY(mote) LOG("%s %s %s %u/%02u/%02u %s %s at %u %s %lu %lu",mote->public?"pub":"pri",mote->beacon?"bekn":"link",mote_tx(mote)?"tx":"rx",mote->priority,mote->txz,mote->rxz,hashname_short(mote->link?mote->link->id:mote->beacon),util_hex(mote->nonce,8,NULL),mote->at,mote_tx(mote)?"tx":"rx",util_frames_outlen(mote->frames),util_frames_inlen(mote->frames));

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
    util_frames_send(m->frames, packet);
    LOG("delivering %d to mote %s",lob_len(packet),hashname_short(link->id));
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
    uint8_t bin[32] = {0};
    base32_decode("publicmeshbeaconaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",0,bin,32);
    if(!(c->beacons = mote_new(c->medium, hashname_vbin(bin)))) return cmnty_free(c);
    c->beacons->beacon = hashname_dup(hashname_vbin(bin));;
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
  if(!tm->pubim) tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
  
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
  
  // start mesh trust to save a discovery step
  link_get(tm->mesh, id);
  
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

/*
* pub bkn tx nonce+self+seed+hash, set at to done
* pub bkn rx, sync at to done, use nonce, order to rx, if hash valid start prv bkn
* 
* prv bkn tx ping nonce+self+seed+hash, set at to done
* prv bkn rx, sync at to done, use nonce, if hash valid then tx handshake
*   else try rx handshake, if no chunks initiate handshake
*/

// fills in next tx knock
tmesh_t tmesh_knock(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");

  MORTY(k->mote);

  // send data frames if any
  if(k->mote->frames)
  {
    if(!util_frames_outbox(k->mote->frames,k->frame))
    {
      // nothing to send, noop
      k->mote->txz++;
      LOG("tx noop %u",k->mote->txz);
      return NULL;
    }

    LOG("TX frame %s\n",util_hex(k->frame,64,NULL));

    // ciphertext full frame
    chacha20(k->mote->secret,k->nonce,k->frame,64);
    return tm;
  }

  // construct beacon tx frame data

  // nonce is prepended to beacons
  memcpy(k->frame,k->nonce,8);

  // copy in hashname
  memcpy(k->frame+8,hashname_bin(tm->mesh->id),32);

  // copy in our reset seed
  memcpy(k->frame+8+32,tm->seed,4);

  // hash the contents
  uint8_t hash[32];
  e3x_hash(k->frame,8+32+4,hash);
  
  // copy as much as will fit for validation
  memcpy(k->frame+8+32+4,hash,64-(8+32+4));

   // ciphertext frame after nonce
  chacha20(k->mote->secret,k->frame,k->frame+8,64-8);

  LOG("TX beacon frame: %s",util_hex(k->frame,64,NULL));

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
    // missed rx windows
    if(!k->tx)
    {
      k->mote->rxz++; // count missed rx knocks
      // if expecting data, trigger a flush
      if(util_frames_await(k->mote->frames)) util_frames_send(k->mote->frames,NULL);
    }
    LOG("knock error");
    if(k->tx) printf("tx error\n");
    return tm;
  }
  
  // tx just updates state things here
  if(k->tx)
  {
    k->mote->txz = 0; // clear skipped tx's
    k->mote->txs++;
    
    // did we send a data frame?
    if(k->mote->frames)
    {
      LOG("tx frame done %lu",util_frames_outlen(k->mote->frames));

      // if beacon mote see if there's a link yet
      if(k->mote->beacon)
      {
        mote_link(k->mote);
      }else{
        // TODO, skip to rx if nothing to send?
      }

      return tm;
    }

    // beacons always sync next at time to when actually done
    k->mote->at = k->stopped;

    // public gets only one priority tx
    if(k->mote->public) k->mote->priority = 0;
    LOG("beacon tx done");
    
      // since there's no ordering w/ public beacons, advance one and make sure we're rx
    if(k->mote->public)
    {
      mote_advance(k->mote);
      if(mote_tx(k->mote)) k->mote->order ^= 1; 
    }
    
    return tm;
  }
  
  // it could be either a beacon or data, check if beacon first
  uint8_t data[64];
  memcpy(data,k->frame,64);
  chacha20(k->mote->secret,k->frame,data+8,64-8); // rx frame has nonce prepended
  
  // hash will validate if a beacon
  uint8_t hash[32];
  e3x_hash(data,8+32+4,hash);
  if(memcmp(data+8+32+4,hash,64-(8+32+4)) == 0)
  {
    memcpy(k->frame,data,64); // deciphered data is correct

    // extract the beacon'd hashname
    hashname_t id = hashname_vbin(k->frame+8);
    if(!id) return LOG("OOM");
    if(hashname_cmp(id,tm->mesh->id) == 0) return LOG("identity crisis");

    // always sync beacon time to sender's reality
    k->mote->at = k->stopped;

    // check the seed to see if there was a reset
    if(memcmp(k->mote->seed,k->frame+8+32,4) == 0) return LOG("skipping seen beacon");
    memcpy(k->mote->seed,k->frame+8+32,4);

    // here is where a public beacon behaves different than a private one
    if(k->mote->public)
    {
      LOG("RX public beacon RSSI %d frame %s",k->rssi,util_hex(k->frame,64,NULL));

      // make sure a private beacon exists
      mote_t private = tmesh_seek(tm, k->mote->medium->com, id);
      if(!private) return LOG("internal error");

      // if the incoming nonce doesnt match, make sure we sync ack
      if(memcmp(k->mote->nonce,k->frame,8) != 0)
      {
        memcpy(k->mote->nonce,k->frame,8);

        // since there's no ordering w/ public beacons, advance one and make sure we're tx
        mote_advance(k->mote);
        if(!mote_tx(k->mote)) k->mote->order ^= 1; 
        k->mote->priority = 1;
        
        LOG("scheduled public beacon at %d with %s",k->mote->at,util_hex(k->mote->nonce,8,NULL));
      }
      
      // sync up private for after
      private->priority = 2;
      private->at = k->mote->at;
      memcpy(private->nonce,k->mote->nonce,8);
      mote_advance(private);
      
      LOG("scheduled private beacon at %d with %s",private->at,util_hex(private->nonce,8,NULL));

      return tm;
    }

    if(hashname_cmp(id,k->mote->beacon) != 0) return LOG("ignoring beacon, expected %s saw %s",hashname_short(k->mote->beacon),id);

    LOG("RX private beacon RSSI %d frame %s",k->rssi,util_hex(k->frame,64,NULL));
    k->mote->last = k->rssi;
    k->mote->rxz = 0;
    k->mote->rxs++;

    // safe to sync to given nonce
    memcpy(k->mote->nonce,k->frame,8);
    k->mote->priority = 3;
    
    // we received a private beacon, initiate handshake
    mote_handshake(k->mote);

    return tm;
  }
  
  // assume sync and process it as a data frame
  
  // if no data handler, initiate a handshake to bootstrap
  if(!k->mote->frames) mote_handshake(k->mote);

  chacha20(k->mote->secret,k->nonce,k->frame,64);
  LOG("RX data RSSI %d frame %s\n",k->rssi,util_hex(k->frame,64,NULL));

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

  if(!util_frames_inbox(k->mote->frames, k->frame))
  {
    k->mote->bad++;
    return LOG("invalid frame: %s",util_hex(k->frame,64,NULL));
  }

  // received stats only after minimal validation
  k->mote->rxz = 0;
  k->mote->rxs++;
  if(k->rssi < k->mote->best || !k->mote->best) k->mote->best = k->rssi;
  if(k->rssi > k->mote->worst) k->mote->worst = k->rssi;
  k->mote->last = k->rssi;

  LOG("RX data received, total %lu rssi %d/%d/%d\n",util_frames_inlen(k->mote->frames),k->mote->last,k->mote->best,k->mote->worst);

  // process any new packets, if beacon mote see if there's a link yet
  if(k->mote->beacon) mote_link(k->mote);
  else mote_process(k->mote);

  return tm;
}

// process everything based on current cycle count, returns success
tmesh_t tmesh_process(tmesh_t tm, uint32_t at, uint32_t rebase)
{
  cmnty_t com;
  mote_t mote;
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

      // skip zip
      if(!mote->z) continue;

      // first rebase cycle count if requested
      if(rebase) mote->at -= rebase;

      // brute timeout idle beacon motes
      if(mote->beacon && mote->rxz > 50) mote_reset(mote);

      // already have one active, noop
      if(knock->ready) continue;

      // move ahead window(s)
      while(mote->at < at) mote_advance(mote);
      while(mote_tx(mote) && mote->frames && !util_frames_outbox(mote->frames,NULL))
      {
        mote->txz++;
        mote_advance(mote);
      }
      MORTY(mote);

      // peek at the next knock details
      memset(&k2,0,sizeof(struct knock_struct));
      mote_knock(mote,&k2);
      
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
    MORTY(knock->mote);

    // do the work to fill in the tx frame only once here
    if(knock->tx && !tmesh_knock(tm, knock))
    {
      LOG("tx prep failed, skipping knock");
      memset(knock,0,sizeof(struct knock_struct));
      continue;
    }

    // signal driver
    if(radio->ready && !radio->ready(radio, knock->mote->medium, knock))
    {
      LOG("radio ready driver failed, canceling knock");
      memset(knock,0,sizeof(struct knock_struct));
      continue;
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
  util_frames_free(m->frames);
  free(m);
  return NULL;
}

mote_t mote_reset(mote_t m)
{
  uint8_t *a, *b, roll[64];
  if(!m || !m->medium) return LOG("bad args");
  tmesh_t tm = m->medium->com->tm;
  LOG("RESET MOTE %p %p\n",m->beacon,m->link);
  // reset to defaults
  m->z = m->medium->z;
  m->txz = m->rxz = 0;
  m->txs = m->rxs = 0;
  m->txhash = m->rxhash = 0;
  m->bad = 0;
  m->cash = 0;
  m->last = m->best = m->worst = 0;
  m->priority = 0;
  m->at = tm->last;
  memset(m->seed,0,4);
  m->frames = util_frames_free(m->frames);
  if(m->link) m->frames = util_frames_new(64);

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

  m->at += next + (m->medium->max*2);
  
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
  k->stop = k->start + m->medium->max;

  // window is small for a link rx
  if(!k->tx && m->link) k->stop = k->start + m->medium->min;

  // derive current channel
  k->chan = m->chan[1] % m->medium->chans;

  // cache nonce
  memcpy(k->nonce,m->nonce,8);

  return m;
}

// initiates handshake over beacon mote
mote_t mote_handshake(mote_t m)
{
  if(!m) return LOG("bad args");
  tmesh_t tm = m->medium->com->tm;

  MORTY(m);

  // TODO, set up first sync timeout to reset!
  util_frames_free(m->frames);
  if(!(m->frames = util_frames_new(64))) return LOG("OOM");
  
  // get relevant link, if any
  link_t link = m->link;
  if(!link) link = mesh_linked(tm->mesh, hashname_char(m->beacon), 0);

  // if public send discovery
  if(m->medium->com->tm->pubim)
  {
    LOG("sending bare discovery %s",lob_json(tm->pubim));
    util_frames_send(m->frames, lob_copy(tm->pubim));
  }else{
    LOG("sending new handshake");
    util_frames_send(m->frames, link_handshakes(link));
  }

  return m;
}

// attempt to establish link from a beacon mote
mote_t mote_link(mote_t mote)
{
  if(!mote) return LOG("bad args");
  tmesh_t tm = mote->medium->com->tm;

  // can't proceed until we've flushed
  if(util_frames_outbox(mote->frames,NULL)) return LOG("not flushed yet: %d",util_frames_outlen(mote->frames));

  // and also have received
  lob_t packet;
  if(!(packet = util_frames_receive(mote->frames))) return LOG("none received yet");

  MORTY(mote);

  // for beacon motes we process only handshakes here and create/sync link if good
  link_t link = NULL;
  do {
    LOG("beacon packet %s",lob_json(packet));
    
    // only receive raw handshakes
    if(packet->head_len == 1)
    {
      link = mesh_receive(tm->mesh, packet, mote->medium->com->pipe);
      continue;
    }

    if(tm->pubim && lob_get(packet,"1a"))
    {
      hashname_t id = hashname_vkey(packet,0x1a);
      if(hashname_cmp(id,mote->beacon) != 0)
      {
        printf("dropping mismatch key %s != %s\n",hashname_short(id),hashname_short(mote->beacon));
        mote_reset(mote);
        return LOG("mismatch");
      }
      // if public, try new link
      lob_t keys = lob_new();
      lob_set_base32(keys,"1a",packet->body,packet->body_len);
      link = link_keys(tm->mesh, keys);
      lob_free(keys);
      lob_free(packet);
      continue;
    }
    
    LOG("unknown packet received on beacon mote: %s",lob_json(packet));
    lob_free(packet);
  } while((packet = util_frames_receive(mote->frames)));
  
  // booo, start over
  if(!link)
  {
    LOG("TODO: if a mote reset its handshake may be old and rejected above, reset link?");
    mote_reset(mote);
    printf("no link\n");
    return LOG("no link found");
  }
  
  if(hashname_cmp(link->id,mote->beacon) != 0)
  {
    printf("link beacon mismatch %s != %s\n",hashname_short(link->id),hashname_short(mote->beacon));
    mote_reset(mote);
    return LOG("mismatch");
  }

  LOG("established link");
  mote_t linked = tmesh_link(tm, mote->medium->com, link);
  mote_reset(linked);
  linked->at = mote->at;
  memcpy(linked->nonce,mote->nonce,8);
  linked->priority = 2;
  link_pipe(link, mote->medium->com->pipe);
  lob_free(link_sync(link));
  
  // stop private beacon, make sure link fail resets it
  mote->z = 0;

  return linked;
}

// process new link data on a mote
mote_t mote_process(mote_t mote)
{
  if(!mote) return LOG("bad args");
  tmesh_t tm = mote->medium->com->tm;
  
  MORTY(mote);

  // process any packets on this mote
  lob_t packet;
  while((packet = util_frames_receive(mote->frames)))
  {
    LOG("pkt %s",lob_json(packet));
    // TODO associate mote for neighborhood
    mesh_receive(tm->mesh, packet, mote->medium->com->pipe);
  }
  
  return mote;
}


knock_t knock_sooner(knock_t a, knock_t b)
{
  LOG("sort a%u %u/%u b%u %u/%u",a->mote->priority,a->start,a->stop,b->mote->priority,b->start,b->stop);
  // any that finish before another starts
  if(a->stop+100 < b->start) return a;
  if(b->stop+100 < a->start) return b;
  // any rx over tx
  if(a->tx < b->tx) return a;
  if(b->tx < a->tx) return a;
  // any higher priority state
  if(a->mote->priority > b->mote->priority) return a;
  if(b->mote->priority > a->mote->priority) return b;
  // firstie
  return (a->start < b->start) ? a : b;
}

