#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

// 5-byte blocks for mesh meta values
typedef struct mblock_struct {
  tmesh_block_t type:3; // medium, at, seq, quality, app
  uint8_t head:4; // per-type
  uint8_t done:1; // last one for cur context
  uint8_t body[4]; // payload
} *mblock_t;

#define MORTY(t,r) LOG_DEBUG("RICK %s %s %s %s [%u,%u,%u,%u,%u] %lu+%lx (%lu/%lu) %u%u %lu",r,t->mote?hashname_short(t->mote->link->id):"selfself",t->do_tx?"TX":"RX",t->frames?"str":"sig",t->itx,t->irx,t->bad,t->miss,t->skip,t->at,t->seq,util_frames_inlen(t->frames),util_frames_outlen(t->frames),t->is_idle,t->do_signal,t->medium);

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
  pipe_free(mote->pipe); // notifies links
  free(mote);
  return LOG_DEBUG("mote free'd");
}

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

/*/////////////////////////////////////////////////////////////////
// standard breakdown based on tempo character
if(tempo->is_signal)
{
  LOG_DEBUG("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
  if(tempo == tm->beacon){
  }else if(tempo == tm->signal){ // shared outgoing signal
  }else if(tempo->mote){ // incoming signal for a mote
  }else{ // bad
  }
}else{
  LOG_DEBUG("stream %s",(tempo == tm->stream)?"shared":"private");
  if(tempo == tm->stream){ // shared stream
  }else if(tempo->mote){ // private stream
  }else{ // bad
  }
}
/*//////////////////////////////////////////////////////////////////

// init any tempo, only called once
static tempo_t tempo_init(tempo_t tempo)
{
  if(!tempo) return LOG_WARN("bad args");
  tmesh_t tm = tempo->tm;
  uint8_t roll[64] = {0};
  if(memcmp(tempo->secret,roll,32)) return LOG_WARN("tempo already initialized");

  // common base secret from community
  e3x_hash((uint8_t*)(tempo->tm->community),strlen(tempo->tm->community),tempo->secret);
  memcpy(roll,tempo->secret,32);

  // standard breakdown based on tempo type
  if(tempo->is_signal)
  {

    LOG_DEBUG("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    tempo_medium(tempo, tm->m_signal);
    if(tempo == tm->beacon)
    {
      tempo->priority = 1; // low
      tempo->do_tx = 1;
      if(!tm->motes) tempo_medium(tempo, tm->m_beacon); // faster when alone
      // nothing extra to add, just copy for roll
      memcpy(roll+32,roll,32);
    }else if(tempo == tm->signal){ // shared outgoing signal
      tempo->priority = 10; // high
      tempo->do_tx = 1;
      // our hashname in rollup
      memcpy(roll+32,hashname_bin(tm->mesh->id),32);
    }else if(tempo->mote){ // incoming signal for a mote
      tempo->priority = 8; // pretty high
      tempo->do_tx = 0;
      // their hashname in rollup
      memcpy(roll+32,hashname_bin(tempo->mote->link->id),32);
    }else{
      return LOG_WARN("unknown signal state");
    }

  }else{ // is stream

    LOG_DEBUG("stream %s",(tempo == tm->stream)?"shared":"private");

    tempo->frames = util_frames_new(64);
    if(tempo == tm->stream) // shared stream
    {
      // the beacon id in the rollup to make shared stream more unique
      memcpy(roll+32,&(tm->beacon_id),4);
    }else if(tempo->mote){
      // combine both hashnames xor'd in rollup
      memcpy(roll+32,hashname_bin(tm->mesh->id),32); // add ours in
      uint8_t i, *bin = hashname_bin(tempo->mote->link->id);
      for(i=0;i<32;i++) roll[32+i] ^= bin[i]; // xor add theirs in
    }else{
      return LOG_WARN("unknown stream state");
    }

  }
  // above states filled in second half of rollup, roll now
  e3x_hash(roll,64,tempo->secret);
  memcpy(roll,tempo->secret,32);
  
  // last roll includes password or fill
  if(tm->password)
  {
    e3x_hash((uint8_t*)(tm->password),strlen(tm->password),roll+32);
    e3x_hash(roll,64,tempo->secret);
  }else{
    e3x_hash(roll,32,tempo->secret);
  }
  
  return tempo;
}

// fills in next tx knock
tempo_t tempo_knock_tx(tempo_t tempo, knock_t knock)
{
  if(!tempo) return LOG_WARN("bad args");
  mote_t mote = tempo->mote;
  tmesh_t tm = tempo->tm;
  uint8_t blocks[60] = {0};
  mblock_t block = NULL;
  uint8_t at = 0;

  if(tempo->is_signal)
  {
    LOG_DEBUG("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){
    }else if(tempo == tm->signal){ // shared outgoing signal
    }else if(tempo->mote){ // incoming signal for a mote
    }else{ // bad
    }
  }else{
    LOG_DEBUG("stream %s",(tempo == tm->stream)?"shared":"private");
    if(tempo == tm->stream){ // shared stream
    }else if(tempo->mote){ // private stream
    }else{ // bad
    }
  }

  // send data frames if any
  if(tempo->frames)
  {
    // first blocks are about us, always send our signal (TODO, dont need to do this on beacon stream)

    block = (mblock_t)(blocks);
    block->type = tmesh_block_medium;
    memcpy(block->body,&(tm->signal->medium),4);

    block = (mblock_t)(blocks+(++at*5));
    block->type = tmesh_block_at;
    uint32_t at_offset = tm->signal->at - tempo->at;
    memcpy(block->body,&(at_offset),4);

    block = (mblock_t)(blocks+(++at*5));
    block->type = tmesh_block_seq;
    memcpy(block->body,&(tm->signal->seq),4);

    // send stream quality in this context
    block = (mblock_t)(blocks+(++at*5));
    block->type = tmesh_block_quality;
    memcpy(block->body,&(tempo->quality),4);
    
    if(tm->app)
    {
      block = (mblock_t)(blocks+(++at*5));
      block->type = tmesh_block_app;
      memcpy(block->body,&(tm->app),4);
    }

    block->done = 1;

    // TODO include signal blocks from last routed-from mote

    // fill in stream frame
    if(!util_frames_outbox(tempo->frames,knock->frame,blocks)) return LOG_WARN("frames failed");

    return tempo;
  }


  // beacon is special
  if(tempo == tm->beacon)
  {
    // first is nonce (unciphered prefixed)
    knock->is_beacon = 1;
    knock->is_tx = 1;
    e3x_rand(knock->nonce,8);
    memcpy(knock->frame,knock->nonce,8);
    
    // then include current random id so others can ignore it
    memcpy(knock->frame+8,&(tm->beacon_id),4);
    
    // then include the medium to switch to to start the stream
    memcpy(knock->frame+12,&(tm->m_stream),4);

    // check hash at end
    murmur(knock->frame,60,knock->frame+60);
    
    return tempo;
  }

  // fill in our outgoing blocks, first with ours
  block = (mblock_t)(blocks);
  block->type = tmesh_block_beacon;
  memcpy(block->body,&(tm->beacon_id),4);

  if(tm->app)
  {
    block = (mblock_t)(blocks+(++at*5));
    block->type = tmesh_block_app;
    memcpy(block->body,&(tm->app),4);
  }

  block->done = 1;

  // fill in blocks for our neighborhood
  uint8_t syncs = 0;
  // TODO, sort by priority
  for(mote=tm->motes;mote && at < 10;mote = mote->next) // must be at least enough space for two blocks
  {
    // lead w/ short hn
    memcpy(blocks+(++at*5),mote->link->id,5);

    // see if there's a ready stream to advertise
    tempo_t stream = mote->stream;
    if(stream && stream->do_signal)
    {
      block = (mblock_t)(blocks+(++at*5));
      if(at >= 12) break;
      block->type = tmesh_block_medium;
      
      // initial stream means send accept
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

    // fill in optional quality and app blocks
    if(mote->signal)
    {
      block = (mblock_t)(blocks+(++at*5));
      if(at >= 12) break;
      block->type = tmesh_block_quality;
      memcpy(block->body,&(mote->stream->quality),4);
    }
    if(mote->app)
    {
      block = (mblock_t)(blocks+(++at*5));
      if(at >= 12) break;
      block->type = tmesh_block_app;
      memcpy(block->body,&(mote->app),4);
    }

    // end this mote's blocks
    block->done = 1;
  }

  // copy in blocks
  memcpy(knock->frame,blocks,60);

  // check hash at end
  murmur(knock->frame,60,knock->frame+60);

  return tempo;
}

// breakout for a dropped tempo
static tempo_t tempo_gone(tempo_t tempo, knock_t knock)
{
  tmesh_t tm = tempo->tm;
  mote_t mote = tempo->mote;

  // reset miss counter
  tempo->miss = 0;

  // this is the beacon, reset it
  if(!mote)
  {
    tempo_beacon(tempo);
    tempo_medium(tempo, tm->m_beacon);
    tempo->frames = util_frames_free(tempo->frames);
    return LOG_DEBUG("beacon reset");
  }
  
  // gone signal == reset their world
  if(mote->signal == tempo)
  {
    tmesh_demote(tm, mote);
    return LOG_DEBUG("mote dropped");
  }

  // drop a gone stream
  mote->stream = tempo_free(mote->stream);
  
  return LOG_DEBUG("mote stream dropped");
}

// breakout for any missed/errored knock for this tempo
static tempo_t tempo_err(tempo_t tempo, knock_t knock)
{
  if(knock->is_tx)
  {
    // failed TX, might be bad if too many?
    tempo->skip++;
    LOG_WARN("failed TX, skip at %lu",tempo->skip);
  }else{
    tempo->miss++;
    LOG_DEBUG("failed RX, miss at %lu",tempo->miss);
    // if expecting data, missed RX triggers a flush
    if(tempo->frames && util_frames_inbox(tempo->frames,NULL,NULL)) util_frames_send(tempo->frames,NULL);
  }

  return tempo;
}

// breakout for a tx knock on this tempo
static tempo_t tempo_knocked_tx(tempo_t tempo, knock_t knock)
{
  if(tempo->is_signal)
  {
    LOG_DEBUG("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){
    }else if(tempo == tm->signal){ // shared outgoing signal
    }else if(tempo->mote){ // incoming signal for a mote
    }else{ // bad
    }
  }else{
    LOG_DEBUG("stream %s",(tempo == tm->stream)?"shared":"private");
    if(tempo == tm->stream){ // shared stream
    }else if(tempo->mote){ // private stream
    }else{ // bad
    }
  }
  
}

// process incoming blocks from this tempo
static tempo_t tempo_blocks(tempo_t tempo, uint8_t *blocks)
{
  mote_t mote = NULL;
  hashname_t id = NULL;
  uint8_t at;
  for(at = 0;at < 12;at++)
  {
    // initial context is always sender
    if(!at)
    {
      mote = tempo->mote;
      id = NULL;
    }else{
      // check given short hash, load mote if known
      hashname_t id = hashname_sbin(blocks+(5*at));
      mote = tmesh_mote(tm,mesh_linkid(tm->mesh, id),NULL);
      at++;
    }

    // loop through blocks for above id/mote
    for(;at < 12;at++)
    {
      mblock_t block = (mblock_t)(blocks+(5*at));
      uint32_t body;
      memcpy(&body,block->body,4);

      switch(block->type)
      {
        case tmesh_block_end:
          LOG_DEBUG("^^^ NONE");
          return tempo;
        case tmesh_block_at:
          if(!mote) break; // require known mote
          LOG_DEBUG("^^^ AT %lu, was %lu set to %lu",body,mote->signal->at,(body + tempo->at));
          mote->signal->at = (body + tempo->at); // is an offset from this tempo
          break;
        case tmesh_block_seq:
          if(!mote) break; // require known mote
          LOG_DEBUG("^^^ SEQ %lu was %lu",body,mote->signal->seq);
          mote->signal->seq = body;
          break;
        case tmesh_block_quality:
          // update for known motes, or let app know for unknown motes
          if(mote) mote->signal->quality = body;
          else if(tm->nearby) tm->nearby(tm, id, block->type, body);
          LOG_DEBUG("^^^ QUALITY %lu",body);
          break;
        case tmesh_block_app:
          if(mote) mote->app = body;
          else if(tm->nearby) tm->nearby(tm, id, block->type, body);
          LOG_DEBUG("^^^ APP %lu",body);
          break;
        case tmesh_block_beacon:
          if(mote) mote->beacon_id = body;
          LOG_DEBUG("^^^ BEACON %lu",body);
          break;
        case tmesh_block_medium:
          if(!mote) break; // require known mote
          switch(block->head)
          {
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
          LOG_WARN("unknown block %u: %s",block->type,util_hex((uint8_t*)block,5,NULL));
      }
    
      // done w/ this mote's blocks
      if(block->done) break;
    }
  }  
}

// breakout for a rx knock on this tempo
static tempo_t tempo_knocked_rx(tempo_t tempo, knock_t knock, uint8_t *blocks)
{
  tmesh_t tm = tempo->tm;

  if(tempo->is_signal)
  {
    LOG_DEBUG("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){
    }else if(tempo == tm->signal){ // shared outgoing signal
    }else if(tempo->mote){ // incoming signal for a mote
    }else{ // bad
    }
  }else{
    LOG_DEBUG("stream %s",(tempo == tm->stream)?"shared":"private");
    if(tempo == tm->stream){ // shared stream
    }else if(tempo->mote){ // private stream
    }else{ // bad
    }
  }

  // received flags/stats first
  tempo->do_signal = 0; // no more advertising
  tempo->miss = 0; // clear any missed counter
  tempo->irx++;
  if(knock->rssi > tempo->best || !tempo->best) tempo->best = knock->rssi;
  if(knock->rssi < tempo->worst || !tempo->worst) tempo->worst = knock->rssi;
  tempo->last = knock->rssi;

  LOG_DEBUG("RX %s %u received, rssi %d/%d/%d data %d\n",tempo->frames?"stream":"signal",tempo->irx,tempo->last,tempo->best,tempo->worst,util_frames_inlen(tempo->frames));

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

  // WIP HERE, MIGRATE ALL UPWARD TO BREAKOUTS

  // driver signal that the tempo is gone
  if(knock->do_gone)
  {
    tempo_gone(tempo, knock);
    return LOG_DEBUG("processed tempo gone");
  }

  if(knock->do_err)
  {
    tempo_err(tempo, knock);
    return LOG_DEBUG("processed knock err");
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
    
    // sent beacons become a stream now, primed w/ a discovery
    if(knock->is_beacon)
    {
      tempo->at = knock->stopped;
      tempo->do_signal = 0;
      tempo->do_schedule = 1;
      tempo->seq = 0;
      tempo->do_tx = 0;
      tempo->frames = util_frames_new(64);
      util_frames_send(tempo->frames,hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys)));

      return NULL;
    }

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

    // the stream following the beacon can only handle the compact discovery packet format
    if(tempo == tm->beacon)
    {
      lob_t packet = util_frames_receive(tempo->frames);
      if(!packet) return NULL;

      // upon receiving an id, create mote and move stream there
      mote_t mote = tmesh_mote(tm, link_key(tm->mesh,packet,0x1a), tempo);
      if(mote)
      {
        // start our signal if none
        if(!tm->signal)
        {
          tm->signal = tempo_new(tm);
          tempo_signal(tm->signal);
          tempo_medium(tm->signal, tm->m_signal);
          e3x_rand((uint8_t*)&(tm->signal->seq),4);
        }
        // follow w/ handshake
        util_frames_send(tempo->frames, link_handshakes(mote->link));
        // new beacon
        tm->beacon = tempo_new(tm);
        tempo_beacon(tm->beacon);
        tempo_medium(tm->beacon, tm->m_signal); // once a link is formed use slower medium
      }else{
        LOG_WARN("unknown packet received from beacon: %s",lob_json(packet));
      }
      lob_free(packet);
      return NULL;
    }

    // received processing only after validation
    tempo_knocked(tempo, knock, meta);

    // signal back if there's full packets waiting
    return (tempo->frames->inbox)?tempo->mote:NULL;
  }

  // decode/validate signal safely
  uint8_t frame[64];
  memcpy(frame,knock->frame,64);
  chacha20(tempo->secret,knock->nonce,frame,64);
  uint32_t check = murmur4(frame,60);

  // if it validates as a signal
  if(memcmp(&check,frame+60,4) == 0)
  {
    // received processing only after validation
    tempo_knocked(tempo, knock, frame);

    // signal any full packets waiting
    return (tempo->frames && tempo->frames->inbox)?tempo->mote:NULL;
  }

  // also check if beacon encoded
  memcpy(frame,knock->frame,64);
  chacha20(tempo->secret,frame,frame+8,64-8);
  check = murmur4(frame,60);
  
  // beacon encoded signal fail
  if(memcmp(&check,frame+60,4) != 0)
  {
    tempo->bad++;
    return LOG_INFO("signal frame validation failed: %s",util_hex(frame,64,NULL));
  }
  
  // always sync beacon at
  tempo->at = knock->stopped;

  MORTY(tempo,"sigbkn");

  uint32_t beacon_id;
  memcpy(&beacon_id,knock->frame+8,4);
  mote_t m;
  for(m = tm->motes;m;m=m->next) if(m->beacon_id == beacon_id)
  {
    return LOG_DEBUG("skipping known beacon from %s",hashname_short(m->link->id));
  }
  
  uint32_t medium;
  memcpy(&medium,knock->frame+12,4);
  if(!medium) return LOG_WARN("no medium on beacon");
  
  // tempo becomes a stream, send discovery
  tempo->do_signal = 0;
  tempo->do_schedule = 1;
  tempo->seq = 0;
  tempo->do_tx = 1; // inverted
  tempo->frames = util_frames_new(64);
  util_frames_send(tempo->frames,hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys)));

  return LOG_DEBUG("beacon initiated stream");
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
  if(tm->knock->is_active) return LOG_WARN("invalid usage, busy knock");

  if(at < tm->at) return LOG_WARN("invalid at in the past");
  tm->at = at;

  LOG_DEBUG("processing for %s at %lu",hashname_short(tm->mesh->id),at);

  // create beacon first time
  if(!tm->beacon)
  {
    tm->beacon = tempo_new(tm);
    tempo_beacon(tm->beacon);
    tempo_medium(tm->beacon, tm->m_beacon);
  }
  
  // start w/ beacon
  tempo_t best = tempo_schedule(tm->beacon, at);

  // upcheck our signal
  if(tm->signal)
  {
    tempo_schedule(tm->signal, at);
    best = tm->sort(tm, best, tm->signal);
  }

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
  if(tm->beacon && !tm->beacon->frames)
  {
    MORTY(tm->beacon,"beacon");

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

// if there's a mote for this link, return it, else create
mote_t tmesh_mote(tmesh_t tm, link_t link, tempo_t stream)
{
  if(!tm || !link) return LOG_WARN("bad args");

  // look for existing and reset stream if given
  mote_t mote;
  for(mote=tm->motes;mote;mote = mote->next) if(mote->link == link)
  {
    if(!stream) return mote;
    if(mote->stream) LOG_WARN("replacing stream for %s",hashname_short(link->id));
    tempo_free(mote->stream);
    mote->stream = stream;
    return mote;
  }

  LOG_INFO("new mote %s",hashname_short(link->id));

  if(!(mote = malloc(sizeof(struct mote_struct)))) return LOG_ERROR("OOM");
  memset(mote,0,sizeof (struct mote_struct));
  mote->link = link;
  mote->tm = tm;
  mote->stream = stream;
  mote->signal = tempo_signal(tempo_new(tm));
  tempo_medium(mote->signal,tm->m_signal);

  // set up pipe
  if(!(mote->pipe = pipe_new("tmesh"))) return mote_free(mote);
  mote->pipe->arg = mote;
  mote->pipe->send = mote_pipe_send;
  
  return mote;
  
}

// drops and free's this mote (link just goes to down state if no other paths)
tmesh_t tmesh_demote(tmesh_t tm, mote_t mote)
{
  if(!tm || !mote) return LOG_WARN("bad args");

  // first remove from list of motes
  if(mote == tm->motes)
  {
    tm->motes = mote->next;
  }else{
    mote_t m;
    for(m=tm->motes;m->next && m->next != mote;m = m->next);
    if(m->next != mote) LOG_WARN("mote %s wasn't in list, this is bad",hashname_short(mote->link->id));
    m->next = mote->next;
  }

  mote_free(mote);
  return tm;
}

// just the basics ma'am
tmesh_t tmesh_new(mesh_t mesh, char *name, char *pass, uint32_t mediums[3])
{
  tmesh_t tm;
  if(!mesh || !name) return LOG_WARN("bad args");

  if(!(tm = malloc(sizeof (struct tmesh_struct)))) return LOG_ERROR("OOM");
  memset(tm,0,sizeof (struct tmesh_struct));

  if(!(tm->knock = malloc(sizeof (struct knock_struct)))) return LOG_ERROR("OOM");
  memset(tm->knock,0,sizeof (struct knock_struct));

  tm->mesh = mesh;
  tm->community = strdup(name);
  if(pass) tm->password = strdup(pass);
  tm->m_beacon = mediums[0];
  tm->m_signal = mediums[1];
  tm->m_stream = mediums[2];

  // seed random beacon id
  e3x_rand((uint8_t*)&(tm->beacon_id),4);
  
  // NOTE: don't create any tempos yet since driver has to add callbacks after this

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
  tempo_free(tm->stream);
  tempo_free(tm->signal);
  tempo_free(tm->beacon);
  free(tm->community);
  if(tm->password) free(tm->password);
  free(tm->knock);
  free(tm);
  return NULL;
}
