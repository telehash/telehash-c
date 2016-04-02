#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

// 5-byte blocks for mesh meta values
typedef struct mblock_struct {
  uint8_t type:3; // medium, at, seq, quality
  uint8_t head:4; // per-type
  uint8_t done:1; // last one
  uint8_t body[4]; // payload
} *mblock_t;

#define MBLOCK_NONE     0
#define MBLOCK_MEDIUM   1
#define MBLOCK_AT       2
#define MBLOCK_SEQ      3
#define MBLOCK_QUALITY  4

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
  tempo->lost = 1;

  return tempo;
}

// update medium if needed
static tempo_t tempo_medium(tempo_t tempo, uint32_t medium)
{
  if(!tempo) return LOG("bad args");

  if(!medium || tempo->medium == medium) return tempo;

  // driver init for any medium changes
  uint32_t revert = tempo->medium;
  tempo->medium = medium;
  if(!tempo->tm->init(tempo->tm, tempo))
  {
    tempo->medium = revert;
    LOG("driver failed medium init from %lu to %lu",revert,medium);
  }else{
    MORTY(tempo,"medium");
  }

  return tempo;
}

// init lost signal tempo
static tempo_t tempo_signal(tempo_t tempo)
{
  if(!tempo) return LOG("bad args");
  
  tempo->signal = 1;
  
  // signal from someone else, or ours out
  hashname_t id = NULL;
  if(tempo->mote)
  {
    tempo->tx = 0;
    tempo->priority = 2; // mid
    id = tempo->mote->link->id;
  }else{
    tempo->tx = 1;
    tempo->priority = 1; // low while lost
    id = tempo->tm->mesh->id;
  }

  // base secret from community name then one hashname
  uint8_t roll[64];
  e3x_hash((uint8_t*)(tempo->tm->community),strlen(tempo->tm->community),roll);
  memcpy(roll+32,hashname_bin(id),32);
  e3x_hash(roll,64,tempo->secret);
  LOG("shared signal secret %s",util_hex(tempo->secret,32,NULL));
  
  MORTY(tempo,"signal");
  return tempo;
}

// init stream tempo
static tempo_t tempo_stream(tempo_t tempo)
{
  if(!tempo || !tempo->mote) return LOG("bad args");
  tmesh_t tm = tempo->tm;
  mote_t to = tempo->mote;

  util_frames_free(tempo->frames);
  tempo->frames = util_frames_new(64);

  // generate mesh-wide unique stream secret
  uint8_t roll[64];
  e3x_hash((uint8_t*)(tm->community),strlen(tm->community),roll);
  memcpy(roll+32,hashname_bin(tm->mesh->id),32); // add ours in
  uint8_t i, *bin = hashname_bin(to->link->id);
  for(i=0;i<32;i++) roll[32+i] ^= bin[i]; // xor add theirs in
  e3x_hash(roll,32+4+4,tempo->secret); // final secret/sync

  MORTY(tempo,"stream");

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
mote_t mote_send(mote_t mote, lob_t packet)
{
  if(!mote) return LOG("bad args");

  if(!mote->stream)
  {
    mote->stream = tempo_new(mote->tm);
    if(!mote->stream)
    {
      LOG("no stream to %s, dropping packet len %lu",hashname_short(mote->link->id),lob_len(packet));
      lob_free(packet);
      return NULL;
    }

    mote->stream->mote = mote;
    tempo_stream(mote->stream);
    tempo_medium(mote->stream, mote->tm->m_stream);
  }

  util_frames_send(mote->stream->frames, packet);
  LOG("delivering %d to mote %s total %lu",lob_len(packet),hashname_short(mote->link->id),util_frames_outlen(mote->stream->frames));
  return mote;
}

// find a stream to send it to for this mote
static void mote_pipe_send(pipe_t pipe, lob_t packet, link_t link)
{
  if(!pipe || !pipe->arg || !packet || !link)
  {
    LOG("bad args");
    lob_free(packet);
    return;
  }

  mote_send((mote_t)pipe->arg, packet);
}

static mote_t mote_free(mote_t mote)
{
  if(!mote) return NULL;
  // free signal and stream
  tempo_free(mote->signal);
  tempo_free(mote->stream);
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
  mote->pipe->send = mote_pipe_send;
  
  return mote;
}

// add a link known to be in this community
mote_t tmesh_find(tmesh_t tm, link_t link, uint32_t m_lost)
{
  mote_t mote;
  if(!tm || !link) return LOG("bad args");

  LOG("finding %s",hashname_short(link->id));

  // check list of motes, add if not there
  for(mote=tm->motes;mote;mote = mote->next) if(mote->link == link) return mote;

  LOG("adding %s",hashname_short(link->id));

  if(!(mote = mote_new(tm, link))) return LOG("OOM");
  mote->link = link;
  mote->next = tm->motes;
  tm->motes = mote;

  // create lost signal
  mote->signal = tempo_new(tm);
  mote->signal->mote = mote;
  tempo_signal(mote->signal);
  tempo_medium(mote->signal, m_lost);

  // this will prime stream so it's advertised
  lob_t out = link_handshakes(mote->link);
  if(!out && tm->discoverable) out = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
  mote_send(mote, out);

  // TODO set up link free event handler to remove this mote
  
  return mote;
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
  if(!tm->signal) tm->signal = tempo_new(tm);
  tempo_signal(tm->signal);
  tempo_medium(tm->signal, medium);
  if(!seq) e3x_rand((uint8_t*)&(tm->signal->seq),4);
  else tm->signal->seq = seq;
  
  return tm->signal;
}

// fills in next tx knock
tempo_t tempo_knock(tempo_t tempo, knock_t knock)
{
  if(!tempo) return LOG("bad args");
  mote_t mote = tempo->mote;
  tmesh_t tm = tempo->tm;
  uint8_t meta[60] = {0};
  mblock_t block = NULL;
  uint8_t at = 0;

  // send data frames if any
  if(!tempo->signal)
  {
    // bundle metadata about our signal in stream meta
    memcpy(meta,tm->mesh->id,5); // our short hn

    block = (mblock_t)(meta+(++at*5));
    block->type = MBLOCK_MEDIUM;
    memcpy(block->body,&(tm->signal->medium),4);

    block = (mblock_t)(meta+(++at*5));
    block->type = MBLOCK_AT;
    memcpy(block->body,&(tm->signal->at),4);

    block = (mblock_t)(meta+(++at*5));
    block->type = MBLOCK_SEQ;
    memcpy(block->body,&(tm->signal->seq),4);
    block->done = 1;

    // TODO, if they're lost suggest non-lost medium
    // TODO include other signals from suggested neighbors

    LOG("META %s",util_hex(meta,60,NULL));

    // fill in stream frame
    if(!util_frames_outbox(tempo->frames,knock->frame,meta))
    {
      // nothing to send, force meta flush
      LOG("outbox empty, sending flush");
      util_frames_send(tempo->frames,NULL);
      util_frames_outbox(tempo->frames,knock->frame,meta);
    }

    return tempo;
  }

  // construct signal meta blocks

  // lost signal is prefixed w/ random nonce in first two blocks
  if(tempo->lost)
  {
    knock->lost = 1;
    e3x_rand(knock->nonce,8);
    memcpy(meta,knock->nonce,8);

    LOG("meta! %s",util_hex(meta,60,NULL));
    // put our signal medium/seq in here for recipient to sync
    memcpy(meta+10,tm->mesh->id,5); // our short hn
    LOG("meta! %s",util_hex(meta,60,NULL));

    block = (mblock_t)(meta+15);
    block->type = MBLOCK_MEDIUM;
    memcpy(block->body,&(tempo->medium),4);
    LOG("meta! %s",util_hex(meta,60,NULL));

    block = (mblock_t)(meta+20);
    block->type = MBLOCK_SEQ;
    memcpy(block->body,&(tempo->seq),4);
    block->done = 1;
    LOG("meta! %s",util_hex(meta,60,NULL));

    at = 5; // next empty block
  }

  LOG("META %u %s",at,util_hex(meta,60,NULL));

  // fill in meta blocks for our neighborhood
  uint8_t syncs = 0;
  // TODO, sort by priority
  for(mote=tm->motes;mote;mote = mote->next)
  {
    LOG("META %u %s",at,hashname_short(mote->link->id));
    // lead w/ short hn
    memcpy(meta+(at*5),mote->link->id,5);

    // see if there's a lost ready stream to advertise
    tempo_t stream = mote->stream;
    LOG("STREAMCHECK %s %s",stream->lost?"lost":"!",util_frames_ready(stream->frames)?"ready":"!!");
    if(stream && stream->lost && util_frames_ready(stream->frames))
    {
      knock->syncs[syncs++] = stream; // needs to be sync'd after tx
      block = (mblock_t)(meta+(++at*5));
      if(at >= 12) break;

      block->type = MBLOCK_MEDIUM;
      block->head = 2; // accept
      memcpy(block->body,&(stream->medium),4);
    }

    // TODO fill in actual quality
    block = (mblock_t)(meta+(++at*5));
    if(at >= 12) break;
    block->type = MBLOCK_QUALITY;
    memcpy(block->body,&(mote->signal->last),2);
    memcpy(block->body+2,&(mote->signal->bad),2);

    // end this mote's blocks
    block->done = 1;
    at++; // next empty block
    if(at >= 12) break;
  }

  LOG("META %s",util_hex(meta,60,NULL));

  // copy in meta
  memcpy(knock->frame,meta,60);

  // check hash at end
  murmur(knock->frame,60,knock->frame+60);

  return tempo;
}

tempo_t tempo_knocked(tempo_t tempo, knock_t knock, uint8_t *meta, uint8_t at)
{
  tmesh_t tm = tempo->tm;
  uint32_t body = 0;
  uint8_t *who = NULL;
  mote_t mote = NULL;

  // received stats first
  tempo->lost = 0;
  tempo->miss = 0;
  tempo->irx++;
  if(knock->rssi > tempo->best || !tempo->best) tempo->best = knock->rssi;
  if(knock->rssi < tempo->worst || !tempo->worst) tempo->worst = knock->rssi;
  tempo->last = knock->rssi;

  LOG("RX %s %u received, rssi %d/%d/%d data %d\n",tempo->signal?"signal":"stream",tempo->irx,tempo->last,tempo->best,tempo->worst,util_frames_inlen(tempo->frames));

  for(;at < 12;at++)
  {
    mblock_t block = (mblock_t)(meta+(5*at));
    LOG("meta block %u: %s",at,util_hex((uint8_t*)block,5,NULL));

    // determine who the following blocks are about
    if(!who)
    {
      who = (uint8_t*)block;
      hashname_t id = hashname_sbin(who);
      link_t link = mesh_linkid(tm->mesh, id);
      if(link)
      {
        // might be a neighbor we know
        mote = tmesh_mote(tm,link);
      }else if(hashname_scmp(id,tm->mesh->id) == 0){
        // if it's about us, use the mote we have for the sender as context
        mote = tempo->mote;
      }
      continue;
    }
    
    memcpy(&body,block->body,4);
    LOG("mblock %u:%u %lu %s",block->type,block->head,body,block->done?"done":"");
    switch(block->type)
    {
      case MBLOCK_NONE:
        return tempo;
      case MBLOCK_AT:
        if(!mote) break; // require known mote
        mote->signal->at = (body - tempo->at); // is an offset
        break;
      case MBLOCK_SEQ:
        if(!mote) break; // require known mote
        mote->signal->seq = body;
        break;
      case MBLOCK_QUALITY:
        // TODO, use/track quality metrics
        LOG("quality %lu for %s",body,mote?hashname_short(mote->link->id):"unknown");
        break;
      case MBLOCK_MEDIUM:
        switch(block->head)
        {
          case 0: // signal medium
            // TODO is senders or request to change ours depending on who
            break;
          case 1: // stream request
            break;
          case 2: // stream accept
            LOG("accepting stream from %s on medium %lu",hashname_short(mote->link->id),body);
            // make sure one exists, is primed, and sync it
            if(!mote_send(mote, NULL)) break; // bad juju
            mote->stream->tx = 0; // we default to inverted since we're accepting
            mote->stream->priority = 3; // little more boost
            mote->stream->at = knock->stopped;
            mote->stream->seq = mote->signal->seq; // TODO invert uint32 for unique starting point
            tempo_medium(mote->stream, body);
            
            break;
          default:
            LOG("unknown medium block %u: %s",block->type,util_hex((uint8_t*)block,5,NULL));
        }
        break;
      default:
        LOG("unknown mblock %u: %s",block->type,util_hex((uint8_t*)block,5,NULL));
    }
    
    // done w/ this who
    if(block->done) who = NULL;
  }
  
  return tempo;
}

// handle a knock that has been sent/received
tmesh_t tmesh_knocked(tmesh_t tm)
{
  if(!tm) return LOG("bad args");
  knock_t knock = tm->knock;
  tempo_t tempo = knock->tempo;

  // always clear skipped counter and ready flag
  tempo->skip = 0;
  knock->ready = 0;

  if(knock->err)
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
    if(tempo->lost) tempo->at = knock->stopped;
    
    // sync any bundled/accepted stream tempos too
    uint8_t syncs;
    for(syncs=0;syncs<5;syncs++) if(knock->syncs[syncs])
    {
      tempo_t sync = knock->syncs[syncs];
      sync->tx = 1; // we are inverted
      sync->priority = 2; // little boost
      sync->at = knock->stopped;
      sync->seq = tempo->seq; // TODO invert uint32 for unique starting point
    }

    MORTY(tempo,"sigout");
    return tm;
  }
  
  // process streams first
  if(!tempo->signal)
  {
    chacha20(tempo->secret,knock->nonce,knock->frame,64);
    LOG("RX data RSSI %d frame %s\n",knock->rssi,util_hex(knock->frame,64,NULL));

    uint8_t meta[50] = {0};
    if(!util_frames_inbox(tempo->frames, knock->frame, meta))
    {
      knock->tempo->bad++;
      return LOG("bad frame: %s",util_hex(knock->frame,64,NULL));
    }

    // received processing only after validation
    tempo_knocked(tempo, knock, meta, 10);

    // process any new packets (TODO, queue for background processing?)
    tempo_process(tempo);

    return tm;
  }

  // decode/validate signal safely
  uint8_t frame[64];
  memcpy(frame,knock->frame,64);
  chacha20(tempo->secret,knock->nonce,frame,64);
  uint32_t check = murmur4(frame,60);
  uint8_t at = 0;

  // did it validate
  if(memcmp(&check,frame+60,4) != 0)
  {
    // also check if lost encoded
    memcpy(frame,knock->frame,64);
    chacha20(tempo->secret,frame,frame+8,64-8);
    check = murmur4(frame,60);
    
    // lost encoded signal fail
    if(memcmp(&check,frame+60,4) != 0)
    {
      tempo->bad++;
      return LOG("signal frame validation failed: %s",util_hex(frame,64,NULL));
    }
    
    // always sync lost at
    tempo->at = knock->stopped;

    MORTY(tempo,"siglost");

    // fall through w/ meta blocks starting after nonce
    at = 2;
  }else{
    MORTY(tempo,"sigfnd");
  }

  // received processing only after validation
  tempo_knocked(tempo, knock, frame, at);

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
    
    // only lost signals are elected for seek
    if(mote->signal->lost) seek = tm->sort(tm, seek, mote->signal);
    
    // any signal is elected for best
    best = tm->sort(tm, best, mote->signal);
    
    if(!mote->stream) continue;
    tempo_schedule(mote->stream, at, rebase);

    // skip held/lost streams that are idle
    if(mote->stream->hold) continue;
    if(mote->stream->lost && !util_frames_ready(mote->stream->frames)) continue;

    // include in best schedule
    best = tm->sort(tm, best, mote->stream);
  }
  
  // already an active knock
  if(tm->knock->ready) return tm;
  
  // try a new knock
  knock_t knock = tm->knock;
  memset(knock,0,sizeof(struct knock_struct));

  // first try seek knock if any
  if(seek && seek->at <= best->at)
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

  // do the tempo-specific work to fill in the tx frame
  if(best->tx)
  {
    if(!tempo_knock(best, knock)) return LOG("knock tx prep failed");
    LOG("TX frame %s\n",util_hex(knock->frame,64,NULL));
    if(knock->lost)
    {
      // nonce is prepended to lost signals unciphered
      chacha20(best->secret,knock->nonce,knock->frame+8,64-8);
    }else{
      chacha20(best->secret,knock->nonce,knock->frame,64);
    }
  }

  knock->ready = 1;

  // signal driver
  if(!tm->schedule(tm))
  {
    memset(knock,0,sizeof(struct knock_struct));
    return LOG("driver schedule failed");
  }

  return tm;
}
