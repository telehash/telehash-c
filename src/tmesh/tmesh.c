#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

// util to return the medium id decoded from the 8-char string
uint32_t tmesh_medium(tmesh_t tm, char *medium)
{
  if(!tm || !medium || strlen(medium < 8)) return 0;
  uint8_t bin[5];
  if(base32_decode(medium,0,bin,5) != 5) return 0;
  uint32_t ret;
  memcpy(&ret,bin+1,4);
  return ret;
}


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
  // TODO motes
  // TODO signal
  if(com->tm->free) com->tm->free(com->tm, NULL, com);
  free(com->name);
  
  free(com);
  return NULL;
}

// create a new blank community
static cmnty_t cmnty_new(tmesh_t tm, char *name, uint32_t medium)
{
  cmnty_t com;
  if(!tm || !name || !medium) return LOG("bad args");

  if(!(com = malloc(sizeof (struct cmnty_struct)))) return LOG("OOM");
  memset(com,0,sizeof (struct cmnty_struct));
  com->tm = tm;
  com->name = strdup(name);

  // try driver init
  if(tm->init && !tm->init(tm, NULL, com)) return cmnty_free(com);

  // TODO make lost signal

  // make official
  com->next = tm->coms;
  tm->coms = com;

  return com;
}

mote_t mote_new(cmnty_t com, link_t link)
{
  // determine ordering based on hashname compare
  uint8_t *bin1 = hashname_bin(id);
  uint8_t *bin2 = hashname_bin(tm->mesh->id);
  for(i=0;i<32;i++)
  {
    if(bin1[i] == bin2[i]) continue;
    m->order = (bin1[i] > bin2[i]) ? 1 : 0;
    break;
  }
  
  // create pipe and signal
  return NULL;
}

//////////////////
// public methods

// join a new community, starts lost signal on given medium
cmnty_t tmesh_join(tmesh_t tm, char *name, char *medium);
{
  uint32_t mid = tmesh_medium(tm, medium);
  cmnty_t com = cmnty_new(tm,name,mid);
  if(!com) return LOG("bad args");

  LOG("joining community %s on medium %s",name,medium);
  lob_t path = lob_new();
  lob_set(path,"type","tmesh");
  lob_set(path,"medium",medium);
  lob_set(path,"name",name);
  tm->mesh->paths = lob_push(tm->mesh->paths, path);

  return c;
}

// leave any community
tmesh_t tmesh_leave(tmesh_t tm, cmnty_t com)
{
  cmnty_t i, cn, c2 = NULL;
  if(!tm || !com) return LOG("bad args");
  
  // snip c out
  for(i=tm->coms;i;i = cn)
  {
    cn = i->next;
    if(i==com) continue;
    i->next = c2;
    c2 = i;
  }
  tm->coms = c2;
  
  cmnty_free(com);
  return tm;
}

// add a link known to be in this community
mote_t tmesh_link(tmesh_t tm, cmnty_t com, link_t link)
{
  mote_t m;
  if(!tm || !com || !link) return LOG("bad args");

  // check list of motes, add if not there
  for(m=com->links;m;m = m->next) if(m->link == link) return m;

  // make sure there's a mote also for syncing
  if(!(tmesh_find(tm, com, link))) return LOG("OOM");

  if(!(m = mote_new(com, link))) return LOG("OOM");
  m->link = link;
  m->next = com->motes;
  com->motes = m;

  // TODO set up link down event handler to remove this mote
  
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
  
  tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
  
  return tm;
}

void tmesh_free(tmesh_t tm)
{
  cmnty_t com, next;
  if(!tm) return;
  for(com=tm->coms;com;com=next)
  {
    next = com->next;
    cmnty_free(com);
  }
  lob_free(tm->pubim);
  // TODO path cleanup
  free(tm);
  return;
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
knock_t tempo_knock(tempo_t tempo, knock_t k)
{
  if(!tempo || !k) return LOG("bad args");
  mote_t mote = tempo->mote;
  cmnty_t com = mote->com;
  tmesh_t tm = com->tm;

  // send data frames if any
  if(tempo->frames)
  {
    if(!util_frames_outbox(tempo->frames,k->frame))
    {
      // nothing to send, noop
      tempo->skip++;
      LOG("tx noop %u",tempo->skip);
      return NULL;
    }

    LOG("TX frame %s\n",util_hex(k->frame,64,NULL));

    // ciphertext full frame
    chacha20(tempo->secret,k->nonce,k->frame,64);
    return k;
  }

  // construct signal tx

  // TODO lost signal
  if(tempo->lost)
  {
    // nonce is prepended to beacons
    memcpy(k->frame,k->nonce,8);

    // copy in self
//    memcpy(k->frame+8,hashname_bin(tm->mesh->id),32);
    
     // ciphertext frame after nonce
    chacha20(k->tempo->secret,k->frame,k->frame+8,64-8);
    return k;
  }

  // TODO normal signal


  LOG("TX signal frame: %s",util_hex(k->frame,64,NULL));

  return k;
}

// handle a knock that has been sent/received
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");
  if(!k->ready) return LOG("knock wasn't ready");
  
  tempo_t tempo = k->tempo;
  mote_t mote = tempo->mote;
  cmnty_t com = mote->com;

  // clear some flags straight away
  k->ready = 0;
  
  if(k->err)
  {
    // missed rx windows
    if(!tempo->tx)
    {
      tempo->miss++; // count missed rx knocks
      // if expecting data, trigger a flush
      if(util_frames_await(tempo->frames)) util_frames_send(tempo->frames,NULL);
    }
    LOG("knock error");
    if(tempo->tx) printf("tx error\n");
    return tm;
  }
  
  // tx just updates state things here
  if(tempo->tx)
  {
    tempo->skip = 0; // clear skipped tx's
    tempo->itx++;
    
    // did we send a data frame?
    if(tempo->frames)
    {
      LOG("tx frame done %lu",util_frames_outlen(tempo->frames));

      // if lost stream tempo see if there's a link yet
      if(tempo->lost)
      {
//        tempo_link(k->tempo);
      }else{
        // TODO, skip to rx if nothing to send?
      }

      return tm;
    }

    // signals always sync next at time to when actually done
    tempo->at = k->stopped;

    return tm;
  }
  
  // process streams first
  if(!tempo->signal)
  {
    chacha20(tempo->secret,k->nonce,k->frame,64);
    LOG("RX data RSSI %d frame %s\n",k->rssi,util_hex(k->frame,64,NULL));

    if(!util_frames_inbox(tempo->frames, k->frame))
    {
      k->tempo->bad++;
      return LOG("bad frame: %s",util_hex(k->frame,64,NULL));
    }

    // received stats only after validation
    tempo->miss = 0;
    tempo->irx++;
    if(k->rssi < tempo->best || !tempo->best) tempo->best = k->rssi;
    if(k->rssi > tempo->worst) tempo->worst = k->rssi;
    tempo->last = k->rssi;

    LOG("RX data received, total %lu rssi %d/%d/%d\n",util_frames_inlen(k->tempo->frames),k->tempo->last,k->tempo->best,k->tempo->worst);

    // process any new packets, if lost tempo see if there's a link yet
    if(tempo->lost) tempo_link(tempo);
    else tempo_process(tempo);
    return tm;
  }

  // process a lost signal
  if(tempo->lost)
  {
    uint8_t data[64];
    memcpy(data,k->frame,64);
    chacha20(tempo->secret,k->frame,data+8,64-8); // rx frame has nonce prepended
    
    // TODO
    // check senders hashname
    // use new seq's
    // sync at time
    // change to !lost

    return tm;
  }
  
  // TODO regular signal
  // decode and validate
  // sync time
  // process stream requests
  // check neighbors for needed paths

  return tm;
}

// inner logic
tempo_t tempo_schedule(tempo_t tempo, uint32_t at, uint32_t rebase)
{
  mote_t mote = tempo->mote;
  cmnty_t com = mote->com;
  tmesh_t tm = com->tm;

  // first rebase cycle count if requested
  if(rebase) tempo->at -= rebase;

  // already have one active, noop
  if(com->knock->ready) return tempo;

  // move ahead window(s)
  while(tempo->at < at)
  {
    // handle seq overflow cascading, notify driver if the big one
    tempo->seq++;
    if(tempo->seq == 0)
    {
      tempo->seq++;
      if(tempo->seq == 0 && tm->notify) tm->notify(tm, NULL);
    }

    // use encrypted seed (follows frame)
    uint8_t seed[64+8] = {0};
    uint8_t nonce[8] = {0};
    memcpy(nonce,&(tempo->medium),4);
    memcpy(nonce+4,&(mote->seq),2);
    memcpy(nonce+6,&(tempo->seq),2);
    chacha20(tempo->secret,nonce,seed,64+8);
    
    // call driver to apply seed to tempo
    if(!tm->advance(tm, tempo, seed+64)) return LOG("driver advance failed");
  }

  return tempo;
}

// process everything based on current cycle count, returns success
tmesh_t tmesh_schedule(tmesh_t tm, uint32_t at, uint32_t rebase)
{
  if(!tm || !at) return LOG("bad args");
  if(!tm->sort || !tm->advance || !tm->schedule) return LOG("driver missing");
  
  LOG("processing for %s at %lu",hashname_short(tm->mesh->id),at);

  // we are looking for the next knock anywhere
  cmnty_t com;
  for(com=tm->coms;com;com=com->next)
  {
    // upcheck our signal first
    tempo_t best = tempo_schedule(com->signal);

    // walk all the tempos for next best knock
    mote_t mote;
    for(mote=com->motes;mote;mote=mote->next)
    {
      // upcheck the mote's signal
      best = tm->sort(best, tempo_schedule(mote->signal));
      
      // TODO, see if this tempo is lost and elect for seek knock

      // any/every stream
      tempo_t tempo;
      for(tempo=mote->streams;tempo;tempo = tempo->next) best = tm->sort(best, tempo_schedule(tempo));
    }
    
    // already one active
    if(com->knock->ready) continue;

    // init new knock for this tempo
    memset(com->knock,0,sizeof(knock_struct));

    // copy nonce parts in
    memcpy(com->knock->nonce,&(tempo->medium),4);
    memcpy(com->knock->nonce+4,&(mote->seq),2);
    memcpy(com->knock->nonce+6,&(tempo->seq),2);

    // do the work to fill in the tx frame only once here
    if(best->tx && !tempo_knock(best, com->knock))
    {
      LOG("tx prep failed, skipping knock");
      continue;
    }
    com->knock->tempo = best;
    com->knock->ready = 1;

    // signal driver
    if(!tm->schedule(tm, com->knock));
    {
      LOG("radio ready driver failed, canceling knock");
      memset(com->knock,0,sizeof(struct knock_struct));
      continue;
    }
  }

  return tm;
}

tempo_t tempo_new(medium_t medium, hashname_t id)
{
  uint8_t i;
  tmesh_t tm;
  tempo_t m;
  
  if(!medium || !medium->com || !medium->com->tm || !id) return LOG("internal error");
  tm = medium->com->tm;

  if(!(m = malloc(sizeof(struct tempo_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct tempo_struct));
  m->at = tm->last;
  m->medium = medium;

  return m;
}

tempo_t tempo_free(tempo_t m)
{
  if(!m) return NULL;
  hashname_free(m->beacon);
  util_frames_free(m->frames);
  free(m);
  return NULL;
}

tempo_t tempo_reset(tempo_t m)
{
  uint8_t *a, *b, roll[64];
  if(!m || !m->medium) return LOG("bad args");
  tmesh_t tm = m->medium->com->tm;
  LOG("RESET tempo %p %p\n",m->beacon,m->link);
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

  // generate tempo-specific secret, roll up community first
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
  
  return m;
}

// initiates handshake over lost stream tempo
tempo_t tempo_handshake(tempo_t m)
{
  if(!m) return LOG("bad args");
  tmesh_t tm = m->medium->com->tm;

  // TODO, set up first sync timeout to reset!
  util_frames_free(m->frames);
  if(!(m->frames = util_frames_new(64))) return LOG("OOM");
  
  // get relevant link, if any
  link_t link = m->link;
  if(!link) link = mesh_linked(tm->mesh, hashname_char(m->beacon), 0);

  // if public and no keys, send discovery
  if(m->medium->com->tm->pubim && (!link || !e3x_exchange_out(link->x,0)))
  {
    LOG("sending bare discovery %s",lob_json(tm->pubim));
    util_frames_send(m->frames, lob_copy(tm->pubim));
  }else{
    LOG("sending new handshake");
    util_frames_send(m->frames, link_handshakes(link));
  }

  return m;
}

// attempt to establish link from a lost stream tempo
tempo_t tempo_link(tempo_t tempo)
{
  if(!tempo) return LOG("bad args");
  tmesh_t tm = tempo->medium->com->tm;

  // can't proceed until we've flushed
  if(util_frames_outbox(tempo->frames,NULL)) return LOG("not flushed yet: %d",util_frames_outlen(tempo->frames));

  // and also have received
  lob_t packet;
  if(!(packet = util_frames_receive(tempo->frames))) return LOG("none received yet");

  // for beacon tempos we process only handshakes here and create/sync link if good
  link_t link = NULL;
  do {
    LOG("beacon packet %s",lob_json(packet));
    
    // only receive raw handshakes
    if(packet->head_len == 1)
    {
      link = mesh_receive(tm->mesh, packet, tempo->medium->com->pipe);
      continue;
    }

    if(tm->pubim && lob_get(packet,"1a"))
    {
      hashname_t id = hashname_vkey(packet,0x1a);
      if(hashname_cmp(id,tempo->beacon) != 0)
      {
        printf("dropping mismatch key %s != %s\n",hashname_short(id),hashname_short(tempo->beacon));
        tempo_reset(tempo);
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
    
    LOG("unknown packet received on beacon tempo: %s",lob_json(packet));
    lob_free(packet);
  } while((packet = util_frames_receive(tempo->frames)));
  
  // booo, start over
  if(!link)
  {
    LOG("TODO: if a tempo reset its handshake may be old and rejected above, reset link?");
    tempo_reset(tempo);
    printf("no link\n");
    return LOG("no link found");
  }
  
  if(hashname_cmp(link->id,tempo->beacon) != 0)
  {
    printf("link beacon mismatch %s != %s\n",hashname_short(link->id),hashname_short(tempo->beacon));
    tempo_reset(tempo);
    return LOG("mismatch");
  }

  LOG("established link");
  tempo_t linked = tmesh_link(tm, tempo->medium->com, link);
  tempo_reset(linked);
  linked->at = tempo->at;
  memcpy(linked->nonce,tempo->nonce,8);
  linked->priority = 2;
  link_pipe(link, tempo->medium->com->pipe);
  lob_free(link_sync(link));
  
  // stop private beacon, make sure link fail resets it
  tempo->z = 0;

  return linked;
}

// process new stream data on a tempo
tempo_t tempo_process(tempo_t tempo)
{
  if(!tempo) return LOG("bad args");
  tmesh_t tm = tempo->mote->com->tm;
  
  // process any packets on this tempo
  lob_t packet;
  while((packet = util_frames_receive(tempo->frames)))
  {
    LOG("pkt %s",lob_json(packet));
    // TODO associate tempo for neighborhood
    mesh_receive(tm->mesh, packet, tempo->mote->pipe);
  }
  
  return tempo;
}

