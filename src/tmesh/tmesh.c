#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

// 10-byte blocks included in signal payloads
typedef struct sigblk_struct {
  uint8_t medium[4];
  uint8_t id[5];
  uint8_t neighbor:1;
  uint8_t val:7;
} *sigblk_t;

// 10-byte blocks included in stream metas
typedef struct stmblk_struct {
  uint8_t medium[4];
  uint8_t at[4];
  uint8_t seq[2];
} *stmblk_t;

#define MORTY(t,r) LOG("RICK %s\t%s %s %s [%u,%u,%u,%u,%u] at:%lu seq:%lx s:%s (%lu/%lu) m:%lu",r,t->mote?hashname_short(t->mote->link->id):"selfself",t->tx?"TX":"RX",t->signal?"signal":"stream",t->itx,t->irx,t->bad,t->miss,t->skip,t->at,t->seq,util_hex(t->secret,4,NULL),util_frames_inlen(t->frames),util_frames_outlen(t->frames),t->medium);

static tempo_t tempo_free(tempo_t tempo)
{
  if(!tempo) return NULL;
  util_frames_free(tempo->frames);
  free(tempo);
  return NULL;
}

static tempo_t tempo_new(tmesh_t tm)
{
  if(!tm) return LOG("bad args");

  tempo_t tempo;
  if(!(tempo = malloc(sizeof(struct tempo_struct)))) return LOG("OOM");
  memset(tempo,0,sizeof (struct tempo_struct));
  tempo->tm = tm;

  return tempo;
}

// new lost signal tempo
static tempo_t tempo_signal(tmesh_t tm, mote_t from, uint32_t medium)
{
  if(!tm) return LOG("bad args");

  tempo_t tempo = tempo_new(tm);
  if(!tempo) return LOG("OOM");

  tempo->lost = 1;
  tempo->signal = 1;
  tempo->medium = medium;
  
  // base secret from community name
  uint8_t roll[64];
  e3x_hash((uint8_t*)(tm->community),strlen(tm->community),roll);
  
  // signal from someone else, or ours out
  if(from)
  {
    tempo->mote = from;
    tempo->tx = 0;
    tempo->priority = 2; // mid
    memcpy(roll+32,hashname_bin(from->link->id),32);
  }else{
    tempo->tx = 1;
    tempo->priority = 1; // low while lost
    memcpy(roll+32,hashname_bin(tm->mesh->id),32);
  }

  e3x_hash(roll,64,tempo->secret);
  LOG("shared signal secret %s",util_hex(tempo->secret,32,NULL));

  // driver init for medium customizations
  if(tm->init && !tm->init(tm, tempo)) return tempo_free(tempo);

  MORTY(tempo,"newsig");

  return tempo;
}

// sync a stream to the current signal it was paired with
static tempo_t tempo_stream_sync(tempo_t stream, tempo_t signal, uint32_t at)
{
  if(!stream || !signal || !at) return LOG("bad args");
  tmesh_t tm = stream->tm;

  // always start at 0/now
  stream->seq = 0;
  stream->at = at;

  // generate mesh-wide unique stream secret
  uint8_t roll[64];
  e3x_hash((uint8_t*)(tm->community),strlen(tm->community),roll);
  memcpy(roll+32,hashname_bin(tm->mesh->id),32); // add ours in
  uint8_t i, *bin = hashname_bin(stream->mote->link->id);
  for(i=0;i<32;i++) roll[32+i] ^= bin[i]; // xor add theirs in

  // hash shared base
  e3x_hash(roll,64,roll);

  // add in current signal uniqueness
  memcpy(roll+32,&(signal->medium),4);
  memcpy(roll+32+4,&(signal->seq),4);
  
  // final secret/sync
  e3x_hash(roll,32+4+4,stream->secret);

  MORTY(stream,"stsync");
  return stream;
}

// get/create stream tempo 
static tempo_t mote_stream(mote_t to, uint32_t medium)
{
  if(!to || !to->signal || !medium) return LOG("bad args");
  tmesh_t tm = to->tm;

  // find any existing
  tempo_t tempo;
  for(tempo = to->streams;tempo;tempo = tempo->next) if(tempo->medium == medium) return tempo;

  // create new
  tempo = tempo_new(tm);
  tempo->next = to->streams;
  to->streams = tempo;

  tempo->mote = to;
  tempo->medium = medium;
  tempo->frames = util_frames_new(64);

  // driver init for medium customizations
  if(tm->init && !tm->init(to->tm, tempo)) return tempo_free(tempo);

  MORTY(tempo,"newstm");

  return tempo;
}

/*
// attempt to establish link from a lost stream tempo
static tempo_t tempo_link(tempo_t tempo)
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
*/

// process new stream data on a tempo
static tempo_t tempo_process(tempo_t tempo)
{
  if(!tempo) return LOG("bad args");
  link_t link = tempo->mote->link;
  
  // process any packets on this tempo
  lob_t packet;
  while((packet = util_frames_receive(tempo->frames)))
  {
    LOG("pkt %s",lob_json(packet));
    // handle our compact discovery packet format
    if(lob_get(packet,"1a"))
    {
      hashname_t id = hashname_vkey(packet,0x1a);
      if(hashname_cmp(id,link->id) != 0)
      {
        printf("dropping mismatch key %s != %s\n",hashname_short(id),hashname_short(link->id));
        lob_free(packet);
        continue;
      }
      // update link keys and trigger handshake
      link_load(link,0x1a,packet);
      util_frames_send(tempo->frames, link_handshakes(link));
      continue;
    }
    
    mesh_receive(link->mesh, packet, tempo->mote->pipe);
  }
  
  return tempo;
}

// find a stream to send it to for this mote
static void mote_send(pipe_t pipe, lob_t packet, link_t link)
{
  if(!pipe || !pipe->arg || !packet || !link)
  {
    LOG("bad args");
    lob_free(packet);
    return;
  }

  mote_t mote = (mote_t)pipe->arg;
  tempo_t stream = mote_stream(mote, mote->tm->m_stream);
  util_frames_send(stream->frames, packet);
  LOG("delivering %d to mote %s total %lu",lob_len(packet),hashname_short(link->id),util_frames_outlen(stream->frames));
}

static mote_t mote_free(mote_t mote)
{
  if(!mote) return NULL;
  // free signals and streams
  tempo_free(mote->signal);
  while(mote->streams)
  {
    tempo_t t = mote->streams;
    mote->streams = mote->streams->next;
    tempo_free(t);
  }
  pipe_free(mote->pipe);
  free(mote);
  return LOG("TODO");
}

static mote_t mote_new(tmesh_t tm, link_t link)
{
  if(!tm || !link) return LOG("bad args");

  LOG("new mote %s",hashname_short(link->id));

  mote_t mote;
  if(!(mote = malloc(sizeof(struct mote_struct)))) return LOG("OOM");
  memset(mote,0,sizeof (struct mote_struct));
  mote->link = link;
  mote->tm = tm;
  
  // set up pipe
  if(!(mote->pipe = pipe_new("tmesh"))) return mote_free(mote);
  mote->pipe->arg = mote;
  mote->pipe->send = mote_send;
  
  return mote;
}

// add a link known to be in this community
mote_t tmesh_find(tmesh_t tm, link_t link, uint32_t m_lost)
{
  mote_t m;
  if(!tm || !link) return LOG("bad args");

  LOG("finding %s",hashname_short(link->id));

  // check list of motes, add if not there
  for(m=tm->motes;m;m = m->next) if(m->link == link) return m;

  LOG("adding %s",hashname_short(link->id));

  if(!(m = mote_new(tm, link))) return LOG("OOM");
  m->link = link;
  m->next = tm->motes;
  tm->motes = m;

  // create lost signal
  m->signal = tempo_signal(tm, m, m_lost);

  // TODO set up link down event handler to remove this mote
  
  return m;
}

// if there's a mote for this link, return it
mote_t tmesh_mote(tmesh_t tm, link_t link)
{
  if(!tm || !link) return LOG("bad args");
  mote_t m;
  for(m=tm->motes;m;m = m->next) if(m->link == link) return m;
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

tmesh_t tmesh_new(mesh_t mesh, char *name, uint32_t mediums[3])
{
  tmesh_t tm;
  if(!mesh || !name) return LOG("bad args");

  if(!(tm = malloc(sizeof (struct tmesh_struct)))) return LOG("OOM");
  memset(tm,0,sizeof (struct tmesh_struct));
  tm->knock = malloc(sizeof (struct knock_struct));
  memset(tm->knock,0,sizeof (struct knock_struct));

  tm->community = strdup(name);
  tm->m_lost = mediums[0];
  tm->m_signal = mediums[1];
  tm->m_stream = mediums[2];
  tm->discoverable = 1; // default true for now
  
  // connect us to this mesh
  tm->mesh = mesh;
  xht_set(mesh->index, "tmesh", tm);
  mesh_on_path(mesh, "tmesh", tmesh_on_path);
  
  return tm;
}

tmesh_t tmesh_free(tmesh_t tm)
{
  if(!tm) return NULL;
  while(tm->motes)
  {
    mote_t m = tm->motes;
    tm->motes = tm->motes->next;
    mote_free(m);
  }
  tempo_free(tm->signal);
  free(tm->community);
  // TODO path cleanup
  free(tm->knock);
  free(tm);
  return NULL;
}

// start/reset our outgoing signal
tempo_t tmesh_signal(tmesh_t tm, uint32_t seq, uint32_t medium)
{
  // TODO allow reset of medium
  tm->signal = tempo_signal(tm, NULL, medium);
  if(!seq) e3x_rand((uint8_t*)&(tm->signal->seq),4);
  else tm->signal->seq = seq;
  
  return tm->signal;
}

// fills in next tx knock
static knock_t tempo_knock(tempo_t tempo)
{
  if(!tempo) return LOG("bad args");
  mote_t mote = tempo->mote;
  tmesh_t tm = tempo->tm;
  knock_t k = tm->knock;

  // send data frames if any
  if(tempo->frames)
  {
    // bundle metadata about our signal
    struct stmblk_struct meta[5] = {{{0},{0},{0}}};
    memcpy(meta[0].medium,&(tm->signal->medium),4);
    memcpy(meta[0].at,&(tm->signal->at),4);
    memcpy(meta[0].seq,&(tm->signal->seq),2);
    // TODO, if they're lost suggest non-lost medium as meta[1]
    // TODO include 3 other signals from last neighbors sent

    // fill in stream frame
    if(!util_frames_outbox(tempo->frames,k->frame,(uint8_t*)meta))
    {
      // nothing to send, force meta flush
      LOG("outbox empty, sending flush");
      util_frames_send(tempo->frames,NULL);
      util_frames_outbox(tempo->frames,k->frame,(uint8_t*)meta);
    }

    LOG("TX frame %s\n",util_hex(k->frame,64,NULL));

    // ciphertext full frame
    chacha20(tempo->secret,k->nonce,k->frame,64);
    return k;
  }

  // construct signal tx

  struct sigblk_struct blk;

  // lost signal is special
  if(tempo->lost)
  {
    // nonce is prepended to lost signals
    memcpy(k->frame,k->nonce,8);
    
    // advertise a stream to other lost motes we know
    uint8_t at = 8, syncs = 0;
    mote_t mote;
    for(mote=tm->motes;mote;mote = mote->next) if(mote->signal->lost)
    {
      // make sure there's a stream to advertise
      tempo_t stream = mote_stream(mote, tm->m_stream);
  
      if(stream->irx) continue; // already active
      memcpy(blk.medium, &(stream->medium), 4);
      memcpy(blk.id, hashname_bin(mote->link->id), 5);
      blk.neighbor = 0;
      blk.val = 2; // lost broadcasts an accept
      k->syncs[syncs++] = stream; // needs to be sync'd after tx
      memcpy(k->frame+at,&blk,10);
      at += 10;
      if(at > 50) break; // full!
    }
    
    // check hash at end
    murmur(k->frame+8,64-(8+4),k->frame+60);

    LOG("TX lost signal frame seq %lu: %s",tempo->seq,util_hex(k->frame,64,NULL));

    // ciphertext frame after nonce
    chacha20(tempo->secret,k->nonce,k->frame+8,64-8);

    return k;
  }

  // TODO normal signal
  for(mote = tm->motes;mote;mote = mote->next) if(!mote->signal->lost)
  {
    // check for any streams needing to sync
    // accept any requested streams (mote->m_req)
    //    tempo->syncs[syncs++] = mote->streams; // needs to be sync'd after tx
  }

  for(mote = tm->motes;mote;mote = mote->next) if(!mote->signal->lost)
  {
    // fill in neighbors in remaining slots
  }

  LOG("TX signal frame: %s",util_hex(k->frame,64,NULL));

  // check hash at end
  murmur(k->frame,60,k->frame+60);

  // ciphertext full frame
  chacha20(tempo->secret,k->nonce,k->frame,64);

  return k;
}

// common successful RX stuff
tempo_t tempo_knocked(tempo_t tempo, knock_t k)
{
  tempo->lost = 0;
  tempo->miss = 0;
  tempo->irx++;
  if(k->rssi > tempo->best || !tempo->best) tempo->best = k->rssi;
  if(k->rssi < tempo->worst || !tempo->worst) tempo->worst = k->rssi;
  tempo->last = k->rssi;

  LOG("RX %s %u received, rssi %d/%d/%d data %d\n",tempo->signal?"signal":"stream",tempo->irx,k->tempo->last,k->tempo->best,k->tempo->worst,util_frames_inlen(k->tempo->frames));

  return tempo;
}

// handle a knock that has been sent/received
tmesh_t tmesh_knocked(tmesh_t tm)
{
  if(!tm) return LOG("bad args");
  knock_t k = tm->knock;
  tempo_t tempo = k->tempo;

  // always clear skipped counter and ready flag
  tempo->skip = 0;
  k->ready = 0;

  if(k->err)
  {
    // missed rx windows
    if(!tempo->tx)
    {
      // if too many missed signal rx, become lost
      tempo->miss++;
      if(tempo->signal && tempo->miss > 10 && !tempo->lost)
      {
        LOG("lost signal to %s",hashname_short(tempo->mote->link->id));
        tempo->lost = 1;
      }

      // if expecting data, trigger a flush
      if(util_frames_await(tempo->frames)) util_frames_send(tempo->frames,NULL);
    }
    LOG("knock %s error, %u misses",tempo->tx?"tx":"rx",tempo->miss);
    return tm;
  }
  
  MORTY(tempo,"knockd");
  
  // tx just updates state things here
  if(tempo->tx)
  {
    tempo->itx++;
    
    // did we send a data frame?
    if(tempo->frames)
    {
      LOG("tx frame done %lu",util_frames_outlen(tempo->frames));

      return tm;
    }

    // lost signals always sync next at time to when actually done
    if(tempo->lost) tempo->at = k->stopped;
    
    // sync any bundled/accepted stream tempos too
    uint8_t syncs;
    for(syncs=0;syncs<5;syncs++) if(k->syncs[syncs])
    {
      tempo_t sync = k->syncs[syncs];
      sync->tx = 1; // we are inverted
      sync->priority = 2; // little boost
      tempo_stream_sync(sync, tempo, k->stopped);
    }

    MORTY(tempo,"sigout");
    return tm;
  }
  
  // process streams first
  if(!tempo->signal)
  {
    chacha20(tempo->secret,k->nonce,k->frame,64);
    LOG("RX data RSSI %d frame %s\n",k->rssi,util_hex(k->frame,64,NULL));

    struct stmblk_struct meta[5] = {{{0},{0},{0}}};
    if(!util_frames_inbox(tempo->frames, k->frame, (uint8_t*)meta))
    {
      k->tempo->bad++;
      return LOG("bad frame: %s",util_hex(k->frame,64,NULL));
    }

    // received stats only after validation
    tempo_knocked(tempo, k);

    // handle any meta
    uint32_t mid = 0, at = 0;
    uint16_t seq = 0;
    memcpy(&mid,meta[0].medium,4);
    if(mid)
    {
      memcpy(&at,meta[0].at,4);
      memcpy(&seq,meta[0].seq,2);
      // TODO sender's signal sync
    }
    memcpy(&mid,meta[1].medium,4);
    if(mid)
    {
      memcpy(&at,meta[0].at,4);
      memcpy(&seq,meta[0].seq,2);
      // TODO our new found signal, validate sender
    }
    // TODO check other meta blocks for neighbor syncs

    // process any new packets (TODO, queue for background processing?)
    tempo_process(tempo);

    return tm;
  }

  // decode/validate signal safely
  uint8_t at = 0;
  uint8_t frame[64];
  memcpy(frame,k->frame,64);
  chacha20(tempo->secret,k->nonce,frame,64);
  uint32_t check = murmur4(frame,60);
  
  // did it validate
  if(memcmp(&check,frame+60,4) != 0)
  {
    // also check if lost encoded
    memcpy(frame,k->frame,64);
    chacha20(tempo->secret,frame,frame+8,64-8);
    uint32_t check = murmur4(frame+8,64-(8+4));
    
    // lost encoded signal fail
    if(memcmp(&check,frame+60,4) != 0)
    {
      tempo->bad++;
      return LOG("signal frame validation failed: %s",util_hex(frame,64,NULL));
    }
    
    // TODO, allow changing mediums?

    // always sync lost time/nonce
    tempo->at = k->stopped;
    memcpy(&(tempo->seq),frame+4,4); // sync tempo nonce

    MORTY(tempo,"siglost");

    // fall through w/ offset past prefixed nonce in frame
    at = 8;
  }else{
    MORTY(tempo,"sigfnd");
  }

  // received stats only after validation
  tempo_knocked(tempo, k);

  mote_t mote = tempo->mote; // NOTE: this is not a crypto-level trust of the sender's authenticity
  struct sigblk_struct blk;
  while(at <= 50)
  {
    memcpy(&blk,frame+at,10);
    at += 10;

    uint32_t medium;
    memcpy(&medium,blk.medium,4);
    hashname_t id = hashname_sbin(blk.id);
    if(medium) LOG("BLOCK %s %lu %s",util_hex((uint8_t*)&blk,10,NULL),medium,hashname_short(id));

    if(blk.neighbor)
    {
      LOG("neighbor %s on %lu q %u",hashname_short(id),medium,blk.val);
      // TODO, do we want to talk to them? add id as a peer path now and do a peer/connect
      continue;
    }

    // TODO also match against our exchange token so neighbors can't track stream requests
    if(hashname_scmp(id,tm->mesh->id) != 0) continue;
    
    // streams r us!
    if(blk.val == 2)
    {
      LOG("accepting stream from %s on medium %lu",hashname_short(mote->link->id),medium);
      // make sure one exists, and sync it
      tempo_t stream = mote_stream(mote, medium);
      stream->tx = 0; // we default to inverted since we're accepting
      stream->priority = 3; // little more boost
      tempo_stream_sync(stream,tempo,k->stopped);

      // if nothing waiting, send something to start the stream
      if(!util_frames_outlen(stream->frames))
      {
        lob_t out = NULL;
        if(!e3x_exchange_out(mote->link->x,0))
        {
          // if no keys, send discovery
          if(!tm->discoverable) return LOG("no keys and not discoverable");
          out = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
          LOG("sending bare discovery %s",lob_json(out));
        }else{
          out = link_handshakes(mote->link);
          LOG("sending handshake");
        }
        util_frames_send(stream->frames, out);
      }

    }
    if(blk.val == 1)
    {
      LOG("stream requested from %s on medium %lu",hashname_short(mote->link->id),medium);
      tempo->mote->m_req = medium;
    }
  }

  return tm;
}

// inner logic
static tempo_t tempo_schedule(tempo_t tempo, uint32_t at, uint32_t rebase)
{
  if(!tempo || !at) return LOG("bad args");
  tmesh_t tm = tempo->tm;

  // initialize to *now* if not set
  if(!tempo->at) tempo->at = at + rebase;

  // first rebase cycle count if requested
  if(rebase) tempo->at -= rebase;

  // already have one active, noop
  if(tm->knock->ready) return tempo;

  // move ahead window(s)
  while(tempo->at <= at)
  {
    tempo->seq++;
    tempo->skip++; // counts missed windows, cleared after knocked

    // use encrypted seed (follows frame)
    uint8_t seed[64+8] = {0};
    uint8_t nonce[8] = {0};
    memcpy(nonce,&(tempo->medium),4);
    memcpy(nonce+4,&(tempo->seq),4);
    chacha20(tempo->secret,nonce,seed,64+8);
    
    // call driver to apply seed to tempo
    if(!tm->advance(tm, tempo, seed+64)) return LOG("driver advance failed");
  }

//  MORTY(tempo,"advncd");

  return tempo;
}

// process everything based on current cycle count, returns success
tmesh_t tmesh_schedule(tmesh_t tm, uint32_t at, uint32_t rebase)
{
  if(!tm || !at) return LOG("bad args");
  if(!tm->sort || !tm->advance || !tm->schedule) return LOG("driver missing");
  if(!tm->signal) return LOG("nothing to schedule");

  LOG("processing for %s at %lu",hashname_short(tm->mesh->id),at);

  // upcheck our signal first
  tempo_t best = tempo_schedule(tm->signal, at, rebase);
  tempo_t seek = NULL;

  // walk all the tempos for next best knock
  mote_t mote;
  for(mote=tm->motes;mote;mote=mote->next)
  {
    // advance
    tempo_schedule(mote->signal, at, rebase);
    
    // lost signals are elected for seek, others for best
    if(mote->signal->lost) seek = tm->sort(tm, seek, mote->signal);
    else best = tm->sort(tm, best, mote->signal);
    
    // any/every stream for best pool
    tempo_t tempo;
    for(tempo=mote->streams;tempo;tempo = tempo->next)
    {
      tempo_schedule(tempo, at, rebase);

//      LOG("stream %s %lu at %lu %u",tempo->tx?"TX":"RX",util_frames_outbox(tempo->frames,NULL),tempo->at,tempo->miss);
      best = tm->sort(tm, best, tempo);
    }
  }
  
  // already an active knock
  if(tm->knock->ready) return tm;
  
  // try a new knock
  knock_t knock = tm->knock;
  memset(knock,0,sizeof(struct knock_struct));

  // first try seek knock if any
  if(seek && seek->at < best->at)
  {
    MORTY(seek,"seekin");

    // copy nonce parts in, nothing to prep since is just RX
    memcpy(knock->nonce,&(seek->medium),4);
    memcpy(knock->nonce+4,&(seek->seq),4);
    knock->tempo = seek;
    knock->ready = 1;
    knock->seekto = best->at;

    // ask driver if it can seek, done if so, else fall through
    if(tm->schedule(tm)) return tm;

    memset(knock,0,sizeof(struct knock_struct));
  }
  
  MORTY(best,"waitin");

  // copy nonce parts in first
  memcpy(knock->nonce,&(best->medium),4);
  memcpy(knock->nonce+4,&(best->seq),4);
  knock->tempo = best;

  // do the work to fill in the tx frame only once here
  if(best->tx && !tempo_knock(best)) return LOG("knock tx prep failed");

  knock->ready = 1;

  // signal driver
  if(!tm->schedule(tm))
  {
    memset(knock,0,sizeof(struct knock_struct));
    return LOG("driver schedule failed");
  }

  return tm;
}
