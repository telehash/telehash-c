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

#define MORTY(t,r) LOG_DEBUG("RICK %s %s %s %s [%u,%u,%u,%u,%u] %lu+%lx (%lu/%lu) %u%u %lu",r,t->mote?hashname_short(t->mote->link->id):"selfself",t->do_tx?"TX":"RX",t->frames?"str":"sig",t->itx,t->irx,t->bad,t->miss,t->skip,t->at,t->seq,util_frames_inlen(t->frames),util_frames_outlen(t->frames),t->is_idle,t->do_signal,t->medium);

static tempo_t tempo_free(tempo_t tempo)
{
  if(!tempo) return NULL;
  util_frames_free(tempo->frames);
  free(tempo);
  return NULL;
}

static tempo_t tempo_new(tmesh_t tm)
{
  if(!tm) return LOG_WARN("bad args");

  tempo_t tempo;
  if(!(tempo = malloc(sizeof(struct tempo_struct)))) return LOG_ERROR("OOM");
  memset(tempo,0,sizeof (struct tempo_struct));
  tempo->tm = tm;

  return tempo;
}

// update medium if needed
static tempo_t tempo_medium(tempo_t tempo, uint32_t medium)
{
  if(!tempo) return LOG_WARN("bad args");

  if(!medium || tempo->medium == medium) return tempo;

  // driver init for any medium changes
  uint32_t revert = tempo->medium;
  tempo->medium = medium;
  if(!tempo->tm->init(tempo->tm, tempo))
  {
    tempo->medium = revert;
    LOG_WARN("driver failed medium init from %lu to %lu",revert,medium);
  }else{
    MORTY(tempo,"medium");
  }

  return tempo;
}

// init signal tempo
static tempo_t tempo_signal(tempo_t tempo)
{
  if(!tempo) return LOG_WARN("bad args");
  
  tempo->priority = 2; // mid

  // signal from someone else, or ours out
  hashname_t id = NULL;
  if(tempo->mote)
  {
    tempo->do_tx = 0;
    id = tempo->mote->link->id;
  }else{
    tempo->do_tx = 1;
    id = tempo->tm->mesh->id;
  }

  // base secret from community name then one hashname
  uint8_t roll[64] = {0};
  e3x_hash((uint8_t*)(tempo->tm->community),strlen(tempo->tm->community),roll);
  if(tm->password) e3x_hash((uint8_t*)(tm->password),strlen(tm->password),roll+32);
  e3x_hash(roll,64,tempo->secret);
  memcpy(roll,tempo->secret,32);
  memcpy(roll+32,hashname_bin(id),32);
  e3x_hash(roll,64,tempo->secret);
  
  MORTY(tempo,"signal");
  return tempo;
}

// init stream tempo
static tempo_t tempo_stream(tempo_t tempo)
{
  if(!tempo || !tempo->mote) return LOG_WARN("bad args");
  tmesh_t tm = tempo->tm;
  mote_t to = tempo->mote;

  // default flags, idle and lost
  tempo->do_signal = 0;
  tempo->do_schedule = 0;
  tempo->do_lost = 1;
  tempo->seq = 0;

  if(!tempo->frames) tempo->frames = util_frames_new(64);
  util_frames_clear(tempo->frames);

  // generate mesh-wide unique stream secret
  uint8_t roll[64] = {0};
  e3x_hash((uint8_t*)(tm->community),strlen(tm->community),roll);
  if(tm->password) e3x_hash((uint8_t*)(tm->password),strlen(tm->password),roll+32);
  e3x_hash(roll,64,tempo->secret);
  memcpy(roll,tempo->secret,32);
  memcpy(roll+32,hashname_bin(tm->mesh->id),32); // add ours in
  uint8_t i, *bin = hashname_bin(to->link->id);
  for(i=0;i<32;i++) roll[32+i] ^= bin[i]; // xor add theirs in
  e3x_hash(roll,64,tempo->secret); // final secret/sync

  MORTY(tempo,"stream");

  return tempo;
}

// (re)init beacon tempo
static tempo_t tempo_beacon(tempo_t tempo)
{
  if(!tempo || tempo->mote) return LOG_WARN("bad args");
  tmesh_t tm = tempo->tm;

  // default flags
  tempo->do_schedule = 1;
  tempo->seq = 0;

  // frames must be empty to start
  if(tempo->frames) util_frames_free(tempo->frame);
  tempo->frames = util_frames_new(64);

  // beacons are mesh-wide shared stream secret
  uint8_t roll[64] = {0};
  e3x_hash((uint8_t*)(tm->community),strlen(tm->community),roll);
  if(tm->password) e3x_hash((uint8_t*)(tm->password),strlen(tm->password),roll+32);
  e3x_hash(roll,64,tempo->secret); // final secret/sync

  MORTY(tempo,"beacon");

  return tempo;
}

// find a stream to send it to for this mote
mote_t mote_send(mote_t mote, lob_t packet)
{
  if(!mote) return LOG_WARN("bad args");
  tempo_t tempo = mote->stream;

  if(!tempo)
  {
    tempo = mote->stream = tempo_new(mote->tm);
    if(!tempo)
    {
      LOG_WARN("no stream to %s, dropping packet len %lu",hashname_short(mote->link->id),lob_len(packet));
      lob_free(packet);
      return NULL;
    }

    tempo->mote = mote;
    tempo_stream(tempo);
    tempo_medium(tempo, mote->tm->m_stream);
  }

  // if not scheduled, make sure signalling
  if(!tempo->do_schedule) tempo->do_signal = 1;
  util_frames_send(tempo->frames, packet);
  LOG_DEBUG("delivering %d to mote %s total %lu",lob_len(packet),hashname_short(mote->link->id),util_frames_outlen(tempo->frames));
  return mote;
}

// find a stream to send it to for this mote
void mote_pipe_send(pipe_t pipe, lob_t packet, link_t recip)
{
  if(!pipe || !pipe->arg || !packet || !recip)
  {
    LOG_WARN("bad args");
    lob_free(packet);
    return;
  }

  mote_t mote = (mote_t)(pipe->arg);

  // send direct
  if(mote->link == recip)
  {
    mote_send(mote, packet);
    return;
  }

  // mote is router, wrap and send to recip via it 
  LOG_DEBUG("routing packet to %s via %s",hashname_short(recip->id),hashname_short(mote->link->id));

  // first wrap routed w/ a head 6 sender, body is orig
  lob_t wrap = lob_new();
  lob_head(wrap,hashname_bin(recip->mesh->id),6); // head is sender, extra 1
  lob_body(wrap,lob_raw(packet),lob_len(packet));
  lob_free(packet);

  // next wrap w/ intended recipient in header
  lob_t wrap2 = lob_new();
  lob_head(wrap2,hashname_bin(recip->id),5); // head is recipient
  lob_body(wrap2,lob_raw(wrap),lob_len(wrap));
  lob_free(wrap);

  mote_send(mote, wrap2);
}

static mote_t mote_free(mote_t mote)
{
  if(!mote) return NULL;
  // free signal and stream
  tempo_free(mote->signal);
  tempo_free(mote->stream);
  pipe_free(mote->pipe);
  free(mote);
  return LOG_WARN("TODO list");
}

static mote_t mote_new(tmesh_t tm, link_t link)
{
  if(!tm || !link) return LOG_WARN("bad args");

  LOG_INFO("new mote %s",hashname_short(link->id));

  mote_t mote;
  if(!(mote = malloc(sizeof(struct mote_struct)))) return LOG_ERROR("OOM");
  memset(mote,0,sizeof (struct mote_struct));
  mote->link = link;
  mote->tm = tm;
  
  // set up pipe
  if(!(mote->pipe = pipe_new("tmesh"))) return mote_free(mote);
  mote->pipe->arg = mote;
  mote->pipe->send = mote_pipe_send;
  
  return mote;
}

// find/link this id, send intro via this mote
mote_t tmesh_intro(tmesh_t tm, hashname_t id, mote_t router)
{
  // check list of motes, add if none
  mote_t mote;
  for(mote=tm->motes;mote;mote = mote->next) if(hashname_scmp(mote->link->id,id) == 0) break;
  if(!mote)
  {
    // get existing or make a barebones link
    link_t link = mesh_linkid(tm->mesh,id);
    if(!link) link = link_get(tm->mesh, id);
    
    if(!(mote = mote_new(tm, link))) return LOG_ERROR("OOM");
    mote->next = tm->motes;
    tm->motes = mote;

    // associate pipe from router as link default
    link_pipe(link, router->pipe);
  }
  
  // send open intro via router
  mote_pipe_send(router->pipe, hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys)), mote->link);

  return mote;
}

// add a link known to be in this community
mote_t tmesh_find(tmesh_t tm, link_t link, uint32_t m_beacon)
{
  mote_t mote;
  if(!tm || !link) return LOG_WARN("bad args");

  LOG_DEBUG("finding %s",hashname_short(link->id));

  // check list of motes, add if not there
  for(mote=tm->motes;mote;mote = mote->next) if(mote->link == link) return mote;

  LOG_INFO("adding %s",hashname_short(link->id));

  if(!(mote = mote_new(tm, link))) return LOG_ERROR("OOM");
  mote->next = tm->motes;
  tm->motes = mote;

  // create lost signal
  mote->signal = tempo_new(tm);
  mote->signal->mote = mote;
  tempo_signal(mote->signal);
  tempo_medium(mote->signal, m_lost);

  // this will prime stream so it's advertised
  lob_t out = NULL;
  if(tm->discoverable) out = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
  // TODO support immediate link_handshakes(mote->link);
  mote_send(mote, out);

  // TODO set up link free event handler to remove this mote
  
  return mote;
}

// if there's a mote for this link, return it
mote_t tmesh_mote(tmesh_t tm, link_t link)
{
  if(!tm || !link) return LOG_WARN("bad args");
  mote_t m;
  for(m=tm->motes;m;m = m->next) if(m->link == link) return m;
  return LOG_WARN("no mote found for link %s",hashname_short(link->id));
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

tmesh_t tmesh_new(mesh_t mesh, char *name, char *pass, uint32_t mediums[3])
{
  tmesh_t tm;
  if(!mesh || !name) return LOG_WARN("bad args");

  if(!(tm = malloc(sizeof (struct tmesh_struct)))) return LOG_ERROR("OOM");
  memset(tm,0,sizeof (struct tmesh_struct));
  tm->knock = malloc(sizeof (struct knock_struct));
  memset(tm->knock,0,sizeof (struct knock_struct));

  tm->community = strdup(name);
  if(pass) tm->password = strdup(pass);
  tm->m_beacon = mediums[0];
  tm->m_signal = mediums[1];
  tm->m_stream = mediums[2];
  tm->discoverable = 1; // default true for now

  // connect us to this mesh
  tm->mesh = mesh;
  xht_set(mesh->index, "tmesh", tm);

  // start discoverable beacon
  tm->beacon = tempo_new(tm);
  tempo_beacon(tm->beacon);
  tempo_medium(tm->beacon, tm->m_beacon);
  
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
  tempo_free(tm->beacon);
  free(tm->community);
  if(tm->password) free(tm->password);
  xht_set(tm->mesh->index, "tmesh", NULL);
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
  if(!tempo) return LOG_WARN("bad args");
  mote_t mote = tempo->mote;
  tmesh_t tm = tempo->tm;
  uint8_t meta[60] = {0};
  mblock_t block = NULL;
  uint8_t at = 0;

  if(tempo == tm->beacon)
  {
    // TODO simple
  }

  // send data frames if any
  if(tempo->frames)
  {
    // bundle metadata about our signal in stream meta
    memcpy(meta,tm->mesh->id,5); // our short hn

    block = (mblock_t)(meta+(++at*5));
    block->type = MBLOCK_MEDIUM;
    memcpy(block->body,&(tm->signal->medium),4);

    block = (mblock_t)(meta+(++at*5));
    block->type = MBLOCK_AT;
    LOG_INFO("tempo %lu signal %lu", tempo->at, tm->signal->at);
    uint32_t at_offset = tm->signal->at - tempo->at;
    memcpy(block->body,&(at_offset),4);

    block = (mblock_t)(meta+(++at*5));
    block->type = MBLOCK_SEQ;
    memcpy(block->body,&(tm->signal->seq),4);
    block->done = 1;

    // TODO, if they're lost suggest non-lost medium
    // TODO include other signals from suggested neighbors

    // fill in stream frame
    if(!util_frames_outbox(tempo->frames,knock->frame,meta)) return LOG_WARN("frames failed");

    return tempo;
  }

  // construct signal meta blocks

  // lost signal is prefixed w/ random nonce in first two blocks
  if(tempo->do_lost)
  {
    knock->is_lost = 1;
    e3x_rand(knock->nonce,8);
    memcpy(meta,knock->nonce,8);

    // put our signal medium/seq in here for recipient to sync
    memcpy(meta+10,tm->mesh->id,5); // our short hn

    block = (mblock_t)(meta+15);
    block->type = MBLOCK_MEDIUM;
    memcpy(block->body,&(tempo->medium),4);

    block = (mblock_t)(meta+20);
    block->type = MBLOCK_SEQ;
    memcpy(block->body,&(tempo->seq),4);
    block->done = 1;

    at = 5; // next empty block
  }

  // fill in meta blocks for our neighborhood
  uint8_t syncs = 0;
  // TODO, sort by priority
  for(mote=tm->motes;mote;mote = mote->next)
  {
    // lead w/ short hn
    memcpy(meta+(at*5),mote->link->id,5);

    // see if there's a lost ready stream to advertise
    tempo_t stream = mote->stream;
    if(stream && stream->do_signal)
    {
      block = (mblock_t)(meta+(++at*5));
      if(at >= 12) break;
      block->type = MBLOCK_MEDIUM;
      
      // force lost motes to direct accept (can't request if lost)
      if(mote->signal->do_lost) stream->seq++;

      // lost stream means send accept
      if(!stream->seq)
      {
        block->head = 1; // request
      }else{
        block->head = 2; // accept
        knock->syncs[syncs++] = stream; // needs to be sync'd after tx
      }

      LOG_DEBUG("signalling %s about a stream %s",hashname_short(mote->link->id),(block->head == 1)?"request":"accept");
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

  // copy in meta
  memcpy(knock->frame,meta,60);

  // check hash at end
  murmur(knock->frame,60,knock->frame+60);

  return tempo;
}

tempo_t tempo_knocked(tempo_t tempo, knock_t knock, uint8_t *meta, uint8_t mblock_pos)
{
  tmesh_t tm = tempo->tm;
  uint32_t body = 0;
  uint8_t *who = NULL;
  mote_t mote = NULL;

  // received flags/stats first
  tempo->do_lost = 0; // we're found
  tempo->do_signal = 0; // no more advertising
  tempo->miss = 0; // clear any missed counter
  tempo->irx++;
  if(knock->rssi > tempo->best || !tempo->best) tempo->best = knock->rssi;
  if(knock->rssi < tempo->worst || !tempo->worst) tempo->worst = knock->rssi;
  tempo->last = knock->rssi;

  LOG_DEBUG("RX %s %u received, rssi %d/%d/%d data %d\n",tempo->frames?"stream":"signal",tempo->irx,tempo->last,tempo->best,tempo->worst,util_frames_inlen(tempo->frames));

  for(;mblock_pos < 12;mblock_pos++)
  {
    mblock_t block = (mblock_t)(meta+(5*mblock_pos));
//    LOG_DEBUG("meta block %u: %s",at,util_hex((uint8_t*)block,5,NULL));

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
      }else{
        // route them a discovery handshake
        LOG("new neighbor, saying hello to %s",hashname_short(id));
        mote = tmesh_intro(tm, id, tempo->mote);
      }
      LOG_DEBUG("BLOCK >>> %s",hashname_short(id));
      continue;
    }
    
    memcpy(&body,block->body,4);
//    LOG_DEBUG("mblock %u:%u %lu %s",block->type,block->head,body,block->done?"done":"");
    switch(block->type)
    {
      case MBLOCK_NONE:
        LOG_DEBUG("^^^ NONE error");
        return tempo;
      case MBLOCK_AT:
        if(!mote) break; // require known mote
        LOG_DEBUG("^^^ AT %lu, was %lu set to %lu",body,mote->signal->at,(body + tempo->at));
        mote->signal->at = (body + tempo->at); // is an offset
        break;
      case MBLOCK_SEQ:
        if(!mote) break; // require known mote
        LOG_DEBUG("^^^ SEQ %lu was %lu",body,mote->signal->seq);
        mote->signal->seq = body;
        break;
      case MBLOCK_QUALITY:
        // TODO, use/track quality metrics
        LOG_DEBUG("^^^ QUALITY %lu",body);
        break;
      case MBLOCK_MEDIUM:
        if(!mote) break; // require known mote
        switch(block->head)
        {
          case 0: // signal medium
            // TODO is senders or request to change ours depending on who
            LOG_DEBUG("^^^ MEDIUM signal %lu",body);
            break;
          case 1: // stream request
            if(!mote_send(mote, NULL)) break; // make sure stream exists
            LOG_DEBUG("^^^ MEDIUM stream request using %lu",body);
            tempo_medium(mote->stream, body);
            mote->stream->do_signal = 1; // start signalling this stream
            mote->stream->do_schedule = 0; // hold stream scheduling activity
            mote->stream->seq++; // any seq signals a stream accept
            break;
          case 2: // stream accept
            // make sure one exists, is primed, and sync it
            if(!mote_send(mote, NULL)) break; // bad juju
            LOG_DEBUG("^^^ MEDIUM stream accept using %lu",body);
            LOG_INFO("accepting stream from %s on medium %lu",hashname_short(mote->link->id),body);
            mote->stream->do_schedule = 1; // go
            mote->stream->do_signal = 0; // done signalling
            mote->stream->do_tx = 0; // we default to inverted since we're accepting
            mote->stream->priority = 3; // little more boost
            mote->stream->at = knock->stopped;
            mote->stream->seq = mote->signal->seq; // TODO invert uint32 for unique starting point
            tempo_medium(mote->stream, body);
            
            break;
          default:
            LOG_WARN("unknown medium block %u: %s",block->type,util_hex((uint8_t*)block,5,NULL));
        }
        break;
      default:
        LOG_WARN("unknown mblock %u: %s",block->type,util_hex((uint8_t*)block,5,NULL));
    }
    
    // done w/ this who
    if(block->done) who = NULL;
  }
  
  return tempo;
}

// handle a knock that has been sent/received, return mote if packets waiting
mote_t tmesh_knocked(tmesh_t tm)
{
  if(!tm) return LOG_WARN("bad args");
  knock_t knock = tm->knock;
  tempo_t tempo = knock->tempo;

  // always clear skipped counter and ready flag
  tempo->skip = 0;
  knock->is_active = 0;

  // driver signal that the tempo is gone
  if(knock->do_gone)
  {
    // clear misses and set lost
    tempo->miss = 0;
    tempo->do_lost = 1;

    // streams change additional states
    if(tempo->frames)
    {
      tempo->do_schedule = 0;
      // signal this stream if still busy
      tempo->do_signal = util_frames_busy(tempo->frames) ? 1 : 0;
      LOG_INFO("gone stream, signal %u",tempo->do_signal);
    }else{
      LOG_INFO("gone signal");
    }
  }

  if(knock->do_err)
  {
    tempo->miss++;

    // if expecting data, trigger a flush
    if(!knock->is_tx && tempo->frames && util_frames_inbox(tempo->frames,NULL,NULL)) util_frames_send(tempo->frames,NULL);
    
    MORTY(tempo,"do_err");
    return NULL;
  }
  
  MORTY(tempo,"knockd");
  
  // tx just updates state things here
  if(knock->is_tx)
  {
    tempo->itx++;
    
    // did we send a data frame?
    if(tempo->frames)
    {
      util_frames_sent(tempo->frames);
      LOG_DEBUG("tx frame done %lu",util_frames_outlen(tempo->frames));

      return NULL;
    }

    // lost signals always sync next at time to when actually done
    if(tempo->do_lost) tempo->at = knock->stopped;
    
    // sync any bundled/accepted stream tempos too
    uint8_t syncs;
    for(syncs=0;syncs<5;syncs++) if(knock->syncs[syncs])
    {
      tempo_t sync = knock->syncs[syncs];
      sync->do_schedule = 1; // start going
      sync->do_tx = 1; // we are inverted
      sync->priority = 2; // little boost
      LOG_DEBUG("advertised stream reset from %lu to %lu",sync->at,knock->stopped);
      sync->at = knock->stopped;
      sync->seq = tempo->seq; // TODO invert uint32 for unique starting point
    }

    MORTY(tempo,"sigout");
    return NULL;
  }
  
  // process streams first
  if(tempo->frames)
  {
    chacha20(tempo->secret,knock->nonce,knock->frame,64);
    LOG_DEBUG("RX data RSSI %d frame %s\n",knock->rssi,util_hex(knock->frame,64,NULL));

    uint8_t meta[60] = {0};
    if(!util_frames_inbox(tempo->frames, knock->frame, meta))
    {
      // if inbox failed but frames still ok, was just mangled bytes
      if(util_frames_ok(tempo->frames))
      {
        knock->tempo->bad++;
        return LOG_INFO("bad frame: %s",util_hex(knock->frame,64,NULL));
      }
      LOG_INFO("stream broken, clearing state");
      util_frames_clear(tempo->frames);
      if(!util_frames_inbox(tempo->frames, knock->frame, meta))
      {
        util_frames_clear(tempo->frames);
        return LOG_WARN("err-clear-err, bad stream frame %s",util_hex(knock->frame,64,NULL));
      }
    }

    // received processing only after validation
    tempo_knocked(tempo, knock, meta, 0);

    if(util_frames_inlen(tempo->frames) && tempo->frames->inbox) return tempo->mote;
    return NULL;
  }

  // decode/validate signal safely
  uint8_t frame[64];
  memcpy(frame,knock->frame,64);
  chacha20(tempo->secret,knock->nonce,frame,64);
  uint32_t check = murmur4(frame,60);
  uint8_t mblock_pos = 0;

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
      return LOG_INFO("signal frame validation failed: %s",util_hex(frame,64,NULL));
    }
    
    // always sync lost at
    tempo->at = knock->stopped;

    MORTY(tempo,"siglost");

    // fall through w/ meta blocks starting after nonce
    mblock_pos = 2;
  }else{
    MORTY(tempo,"sigfnd");
  }

  // received processing only after validation
  tempo_knocked(tempo, knock, frame, mblock_pos);

  if(util_frames_inlen(tempo->frames) && tempo->frames->inbox) return tempo->mote;
  return NULL;
}

// inner logic
static tempo_t tempo_schedule(tempo_t tempo, uint32_t at)
{
  if(!tempo || !at) return LOG_WARN("bad args");
  tmesh_t tm = tempo->tm;

  // initialize to *now* if not set
  if(!tempo->at) tempo->at = at;

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
    if(!tm->advance(tm, tempo, seed+64)) return LOG_WARN("driver advance failed");
  }

//  MORTY(tempo,"advncd");

  return tempo;
}

// process everything based on current cycle count, returns success
tmesh_t tmesh_schedule(tmesh_t tm, uint32_t at)
{
  if(!tm || !at) return LOG_WARN("bad args");
  if(!tm->sort || !tm->advance || !tm->schedule) return LOG_ERROR("driver missing");
  if(!tm->signal) return LOG_INFO("nothing to schedule");
  if(tm->knock->is_active) return LOG_WARN("invalid usage, busy knock");

  if(at < tm->at) return LOG_WARN("invalid at in the past");
  tm->at = at;

  LOG_DEBUG("processing for %s at %lu",hashname_short(tm->mesh->id),at);

  // upcheck our signal first
  tempo_t best = tempo_schedule(tm->signal, at);

  // walk all the tempos for next best knock
  mote_t mote;
  for(mote=tm->motes;mote;mote=mote->next)
  {
    // advance signal, always elected for best
    if(mote->signal)
    {
      tempo_schedule(mote->signal, at);
      best = tm->sort(tm, best, mote->signal);
    }
    
    // advance stream too, election depends on state (rx or !idle)
    if(mote->stream)
    {
      tempo_schedule(mote->stream, at);
      if(!mote->stream->do_tx || !mote->stream->is_idle) best = tm->sort(tm, best, mote->stream);
    }
  }
  
  // try a new knock
  knock_t knock = tm->knock;
  memset(knock,0,sizeof(struct knock_struct));

  // first try beacon seekin ;)
  if(tm->beacon)
  {
    MORTY(seek,"beacon");

    // copy nonce parts in, nothing to prep since is just RX
    memcpy(knock->nonce,&(tm->beacon->medium),4);
    memcpy(knock->nonce+4,&(tm->beacon->seq),4);
    knock->tempo = tm->beacon;
    knock->seekto = best->at;

    // ask driver if it can seek, done if so, else fall through
    knock->is_active = 1;
    if(tm->schedule(tm)) return tm;
    memset(knock,0,sizeof(struct knock_struct));
  }
  
  MORTY(best,"schdld");

  // copy nonce parts in first
  memcpy(knock->nonce,&(best->medium),4);
  memcpy(knock->nonce+4,&(best->seq),4);
  knock->tempo = best;

  // do the tempo-specific work to fill in the tx frame
  if(best->do_tx)
  {
    if(!tempo_knock(best, knock)) return LOG_WARN("knock tx prep failed");
    knock->is_tx = 1;
    LOG_DEBUG("TX frame %s\n",util_hex(knock->frame,64,NULL));
    if(knock->is_beacon)
    {
      // nonce is prepended to beacons unciphered
      chacha20(best->secret,knock->nonce,knock->frame+8,64-8);
    }else{
      chacha20(best->secret,knock->nonce,knock->frame,64);
    }
  }

  // signal driver
  knock->is_active = 1;
  if(!tm->schedule(tm))
  {
    memset(knock,0,sizeof(struct knock_struct));
    return LOG_WARN("driver schedule failed");
  }

  return tm;
}

tmesh_t tmesh_rebase(tmesh_t tm, uint32_t at)
{
  if(!tm || !at) return LOG_WARN("bad args");
  if(at > tm->at) return LOG_WARN("invalid at in the future");
  if(!tm->signal) return LOG_INFO("nothing to rebase");
  if(tm->knock->is_active) return LOG_WARN("invalid usage, busy knock");

  LOG_DEBUG("rebasing for %s at %lu",hashname_short(tm->mesh->id),at);

  tm->signal->at -= at;

  mote_t mote;
  for(mote=tm->motes;mote;mote=mote->next)
  {
    mote->signal->at -= at;
    if(mote->stream) mote->stream->at -= at;
  }
  
  return tm;
}
  