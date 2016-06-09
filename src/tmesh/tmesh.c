#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

// blocks are 32bit mesh metadata exchanged opportunistically
typedef enum {
  tmesh_block_end = 0, // no more blocks
  tmesh_block_medium, // current signal medium
  tmesh_block_at, // next signal time from now
  tmesh_block_seq, // next signal sequence#
  tmesh_block_qos, // last known signal quality
  tmesh_block_app // app defined
} tmesh_block_t;

// 5-byte blocks for mesh meta values
typedef struct mblock_struct {
  tmesh_block_t type:3; // medium, at, seq, quality, app
  uint8_t head:4; // per-type
  uint8_t done:1; // last one for cur context
  uint8_t body[4]; // payload
} *mblock_t;

#define STATED(t) util_sys_log(7, "", __LINE__, __FUNCTION__, \
      "\t%s %s %u/%d/%u %s %d", \
        t->mote?hashname_short(t->mote->link->id):(t==t->tm->signal)?"mysignal":(t==t->tm->beacon)?"mybeacon":"myshared", \
        t->state.is_signal?(t->mote?"<-":"->"):"<>", \
        t->medium, \
        t->last, \
        t->priority, \
        t->state.accepting?"A":(t->state.requesting?"R":(util_frames_busy(t->frames)?"B":"I")), \
        t->frames?util_frames_outlen(t->frames):-1);


// forward-ho
static tempo_t tempo_new(tmesh_t tm);
static tempo_t tempo_free(tempo_t tempo);
static tempo_t tempo_init(tempo_t tempo);
static tempo_t tempo_medium(tempo_t tempo, uint32_t medium);
static tempo_t tempo_gone(tempo_t tempo);

// return current mote appid
uint32_t mote_appid(mote_t mote)
{
  if(!mote) return 0;
  return mote->app;
}

// find a stream to send it to for this mote
mote_t mote_send(mote_t mote, lob_t packet)
{
  if(!mote) return LOG_WARN("bad args");
  tempo_t tempo = mote->stream;

  if(!tempo)
  {
    LOG_DEBUG("send initiated new stream");
    tempo = mote->stream = tempo_new(mote->tm);
    tempo->state.is_stream = 1;
    if(packet)
    {
      tempo->state.requesting = 1;
      mote->signal->state.adhoc = 1; // try adhoc signal for fast stream init
    }
    tempo->mote = mote;
    tempo_init(tempo);
  }

  // a NULL packet here would trigger a flush, but semantics of mote_send use NULL to ensure stream exists (TODO detangle!)
  if(packet)
  {
    if(util_frames_outlen(tempo->frames) > 1000)
    {
      lob_free(packet);
      return LOG_WARN("stream outbox full (%lu), dropping packet",util_frames_outlen(tempo->frames));
    }
    util_frames_send(tempo->frames, packet);
    LOG_DEBUG("delivering %d to mote %s total %lu",lob_len(packet),hashname_short(mote->link->id),util_frames_outlen(tempo->frames));
  }

  return mote;
}

// send this packet to this id via this router
mote_t mote_route(mote_t router, hashname_t to, lob_t packet)
{
  if(!router || !to || !packet) return LOG_WARN("bad args");
  tmesh_t tm = router->tm;

  // mote is router, wrap and send to recip via it 
  LOG_CRAZY("routing packet to %s via %s",hashname_short(to),hashname_short(router->link->id));

  // first wrap routed w/ a head 6 sender, body is orig
  lob_t wrap = lob_new();
  lob_head(wrap,hashname_bin(tm->mesh->id),6); // head is sender, extra 1
  lob_body(wrap,lob_raw(packet),lob_len(packet));
  lob_free(packet);

  // next wrap w/ intended recipient in header
  lob_t wrap2 = lob_new();
  lob_head(wrap2,hashname_bin(to),5); // head is recipient
  lob_body(wrap2,lob_raw(wrap),lob_len(wrap));
  lob_free(wrap);

  return mote_send(router, wrap2);
}

// find a stream to send it to for this mote
link_t mote_pipe_send(link_t recip, lob_t packet, void *arg)
{
  if(!recip || !arg)
  {
    lob_free(packet);
    return LOG_WARN("bad args");
  }
  if(!packet) return LOG_DEBUG("TODO: handle a link/mote down logic in one place");

  mote_t mote = (mote_t)(arg);

  // send along direct or routed
  if(mote->link == recip) mote_send(mote, packet);
  else mote_route(mote, recip->id, packet);

  return recip;
}

static mote_t mote_free(mote_t mote)
{
  if(!mote) return NULL;
  LOG_INFO("freeing mote %s",hashname_short(mote->link->id));
  // free signal and stream
  tempo_free(mote->signal);
  tempo_free(mote->stream);
  link_down(mote->link);
  free(mote);
  return LOG_CRAZY("mote free'd");
}

// new blank mote, caller must attach stream and link_pipe
static mote_t mote_new(tmesh_t tm, link_t link)
{
  mote_t mote;
  if(!(mote = malloc(sizeof(struct mote_struct)))) return LOG_ERROR("OOM");
  memset(mote,0,sizeof (struct mote_struct));
  mote->link = link;
  mote->tm = tm;
  mote->next = tm->motes;
  tm->motes = mote;
  
  LOG_INFO("new mote %s",hashname_short(link->id));
  
  // bind new signal
  mote->signal = tempo_new(tm);
  mote->signal->state.is_signal = 1;
  mote->signal->mote = mote;
  tempo_init(mote->signal);

  STATED(mote->signal);

  return mote;
}

static tempo_t tempo_free(tempo_t tempo)
{
  if(!tempo) return NULL;
  if(tempo->tm->knock->tempo == tempo) tempo->tm->knock->tempo = NULL; // safely cancels an existing knock
  tempo->medium = tempo->priority = 0;
  STATED(tempo);
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
  tmesh_t tm = tempo->tm;
  if(!tm->medium) return LOG_ERROR("driver missing");

  // no-op if medium is set and no change is requested
  if(tempo->medium && (tempo->medium == medium)) return tempo;

  // create a stable seed unique to this tempo for medium to use
  uint8_t seed[8] = {0};
  chacha20(tempo->secret,seed,seed,8);
  
  if(!tm->medium(tm, tempo, seed, medium)) return LOG_WARN("driver failed medium %lu",medium);

  return tempo;
}

/*/////////////////////////////////////////////////////////////////
// standard breakdown based on tempo character
if(tempo->state.is_signal)
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

  // common base secret from community
  e3x_hash((uint8_t*)(tempo->tm->community),strlen(tempo->tm->community),tempo->secret);
  memcpy(roll,tempo->secret,32);

  // standard breakdown based on tempo type
  if(tempo->state.is_signal)
  {

    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));

    if(tempo == tm->beacon)
    {
      tempo->priority = 1; // low
      e3x_rand((uint8_t*)&(tempo->seq),4); // random sequence
      tempo->c_tx = tempo->state.seen = tempo->state.adhoc = 0; // ensure clear
      tempo->mote = NULL; // only used as a cache
      // nothing extra to add, just copy for roll
      memcpy(roll+32,roll,32);
    }else if(tempo == tm->signal){ // shared outgoing signal
      tempo->priority = 7; // high
      // our hashname in rollup
      memcpy(roll+32,hashname_bin(tm->mesh->id),32);
    }else if(tempo->mote){ // incoming signal for a mote
      tempo->priority = 8; // pretty high
      // their hashname in rollup
      memcpy(roll+32,hashname_bin(tempo->mote->link->id),32);
    }else{
      return LOG_WARN("unknown signal state");
    }

  }else{ // is stream

    LOG_CRAZY("stream %s",(tempo == tm->stream)?"shared":"private");

    if(tempo->frames) util_frames_free(tempo->frames);
    tempo->frames = util_frames_new(64);
    if(tempo == tm->stream) // shared stream
    {
      e3x_rand((uint8_t*)&(tempo->seq),4); // random sequence
      // combine both short hashnames in the rollup to make shared stream more unique
      memcpy(roll+32,hashname_bin(tm->mesh->id),5); // add ours in
      uint8_t i;
      for(i=0;i<5;i++) roll[32+i] ^= tm->seen[i]; // xor add theirs in
      // shared stream always has a discovery primed
      util_frames_send(tempo->frames,hashname_im(tm->mesh->keys, 0x1a));
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
  
  // tell driver to set the medium
  tempo_medium(tempo, 0);
  
  STATED(tempo);
  
  return tempo;
}

// special tx fill for adhoc signal beacon
tempo_t tempo_knock_adhoc(tempo_t signal, knock_t knock)
{
  tmesh_t tm = signal->tm;
  tempo_t stream = signal->mote->stream;

  // first is nonce (unciphered prefixed)
  e3x_rand(knock->nonce,8); // random nonce each time
  memcpy(knock->frame,knock->nonce,8);

  uint8_t index = 2;

  // block pattern is [them][us][stream blocks]
  memcpy(knock->frame+(index*5),hashname_bin(signal->mote->link->id),5);
  index++;
  memcpy(knock->frame+(index*5),hashname_bin(tm->mesh->id),5);
  index++;
  
  // NOTE duplicated from normal signal tx (ick)
  mblock_t block = (mblock_t)(knock->frame+(index*5));
  block->type = tmesh_block_medium;
  memcpy(block->body,&(stream->medium),4);
  if(stream->state.requesting) block->head = 1;
  if(stream->state.accepting)
  {
    block->head = 2;
    // try starting stream now
    stream->state.accepting = stream->state.requesting = 0; // unleash the stream
    stream->state.direction = 0; // we are inverted
    stream->priority = 3; // little boost
    stream->seq = signal->seq;
    // ->at is sync'd in knocked_tx
  }

  block->done = 1;
  murmur(knock->frame,60,knock->frame+60);
  
  LOG_INFO("ad-hoc signal to %s about a stream %s",hashname_short(signal->mote->link->id),(block->head == 1)?"request":"accept");
  return signal;
}

// fills in next tx knock
tempo_t tempo_knock_tx(tempo_t tempo, knock_t knock)
{
  if(!tempo) return LOG_WARN("bad args");
  mote_t mote = tempo->mote;
  tmesh_t tm = tempo->tm;
  uint8_t blocks[60] = {0};
  mblock_t block = NULL;
  uint8_t index = 0;

  memset(knock->frame,0,64);
  if(tempo->state.is_signal)
  {
    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){

      // first is nonce (unciphered prefixed)
      e3x_rand(knock->nonce,8); // random nonce each time
      memcpy(knock->frame,knock->nonce,8);

      // first block is always our short hashname
      memcpy(blocks,hashname_bin(tm->mesh->id),5);

      // then blocks about our beacon for anyone to sync to, we'll RX after one c_tx
      block = (mblock_t)(blocks+(++index*5));
      block->type = tmesh_block_medium;
      memcpy(block->body,&(tempo->medium),4);
      block = (mblock_t)(blocks+(++index*5));
      block->type = tmesh_block_seq;
      memcpy(block->body,&(tempo->seq),4);
      block->done = 1;

      // include an accept for a shared stream just once
      if(tempo->state.seen)
      {
        tempo->state.seen = 0;

        // lead w/ seen short hn
        memcpy(blocks+(++index*5),tm->seen,5);

        // init/create new shared stream, starts immediately, ->at is set after TX
        if(tm->stream) return LOG_ERROR("attempt to beacon when shared stream is active");
        tm->stream = tempo_new(tm);
        tm->stream->state.is_stream = 1;
        tempo_init(tm->stream);
        tm->stream->state.direction = 0; // we are inverted

        // then blocks about our shared stream, just medium/seq
        block = (mblock_t)(blocks+(++index*5));
        block->type = tmesh_block_medium;
        block->head = 2; // immediate accepting
        memcpy(block->body,&(tm->stream->medium),4);
        block = (mblock_t)(blocks+(++index*5));
        block->type = tmesh_block_seq;
        memcpy(block->body,&(tm->stream->seq),4);
        block->done = 1;
        
      }
      
      memcpy(knock->frame+10,blocks,50);

    }else if(tempo == tm->signal){ // shared outgoing signal

      // fill in our outgoing blocks, just cur app value for now
      block = (mblock_t)(blocks);
      block->type = tmesh_block_app;
      memcpy(block->body,&(tm->app),4);

      block->done = 1;

      // fill in blocks for our neighborhood
      for(mote=tm->motes;mote && index < 10;mote = mote->next) // must be at least enough space for two blocks
      {
        // only include motes that want to be
        bool do_qos = false;
        bool do_stream = false;
        if(mote->signal->state.qos_ping || mote->signal->state.qos_pong) do_qos = true;
        if(mote->stream && (mote->stream->state.requesting || mote->stream->state.accepting)) do_stream = true;
        if(!(do_qos || do_stream)) continue;

        // lead w/ short hn
        memcpy(blocks+(++index*5),mote->link->id,5);

        // a ready stream to signal about
        if(do_stream)
        {
          tempo_t stream = mote->stream;
          block = (mblock_t)(blocks+(++index*5));
          if(index >= 12) break;
          block->type = tmesh_block_medium;
      
          // requesting is easy
          if(stream->state.requesting)
          {
            block->head = 1;
          }

          // accepting changes state
          if(stream->state.accepting)
          {
            block->head = 2;
            // start stream when accept is sent
            stream->state.accepting = stream->state.requesting = 0; // just in case
            stream->state.direction = 0; // we are inverted
            stream->priority = 3; // little boost
            stream->at = tempo->at;
            stream->seq = tempo->seq; // TODO invert uint32 for unique starting point
            STATED(tempo);
          }

          stream->state.adhoc = 0; // just in case, don't do this anymore
          LOG_INFO("signalling %s about a stream %s(%d)",hashname_short(mote->link->id),(block->head == 1)?"request":"accept",block->head);
          memcpy(block->body,&(stream->medium),4);
        }

        // qos mode too
        if(do_qos)
        {
          block = (mblock_t)(blocks+(++index*5));
          if(index >= 12) break;
          block->type = tmesh_block_qos;
          memcpy(block->body,&(mote->signal->qos_local),4);
      
          if(mote->signal->state.qos_ping) block->head = 1;
          if(mote->signal->state.qos_pong)
          {
            block->head = 2;
            mote->signal->state.qos_pong = 0; // clear since sent
          }
        }

        // always bundle app id to confirm and for ambiance in the neighborhood
        block = (mblock_t)(blocks+(++index*5));
        if(index >= 12) break;
        block->type = tmesh_block_app;
        memcpy(block->body,&(mote->app),4);

        // end this mote's blocks
        block->done = 1;
      }

      memcpy(knock->frame,blocks,60);

    }else{ // bad
      return LOG_WARN("unknown signal state");
    }
    
    // all signals check-hash
    murmur(knock->frame,60,knock->frame+60);
    
  }else{
    LOG_CRAZY("stream %s",(tempo == tm->stream)?"shared":"private");
    if(tempo == tm->stream){ // shared stream
      // no blocks
    }else if(tempo->mote){ // private stream

      // only blocks are about us, always send our signal
      tempo_t about = tm->signal;
      block = (mblock_t)(blocks);
      block->type = tmesh_block_medium;
      memcpy(block->body,&(about->medium),4);
      block = (mblock_t)(blocks+(5));
      block->type = tmesh_block_at;
      uint32_t at_offset = about->at - tempo->at;
      memcpy(block->body,&(at_offset),4);
      block = (mblock_t)(blocks+(5+5));
      block->type = tmesh_block_seq;
      memcpy(block->body,&(about->seq),4);

      // send stream quality in this context
      block = (mblock_t)(blocks+(5+5+5));
      block->type = tmesh_block_qos;
      memcpy(block->body,&(tempo->qos_local),4);
    
      block = (mblock_t)(blocks+(5+5+5+5));
      block->type = tmesh_block_app;
      memcpy(block->body,&(tm->app),4);

      block->done = 1;

      // TODO include signal blocks from last routed-from mote as bootstrapping hint
    }else{ // bad
      return LOG_WARN("unknown stream state %p",tempo);
    }

    // all streams: fill in frame
    if(!util_frames_outbox(tempo->frames,knock->frame,blocks)) return LOG_WARN("frames failed");
    
  }

  return tempo;
}

// process incoming blocks from this tempo
static tempo_t tempo_blocks_rx(tempo_t tempo, uint8_t *blocks, uint8_t index)
{
  tmesh_t tm = tempo->tm;
  mote_t about;
  mote_t from; // when about is us
  struct hashname_struct hn_val;
  hashname_t seen;

  for(;index < 12;index++)
  {
    about = from = NULL;
    seen = NULL;

    // initial about is always sender
    if(!index)
    {
      about = tempo->mote;
    }else{

      // 0's not a valid hn
      uint8_t zeros[5] = {0};
      if(memcmp(zeros,blocks+(5*index),5) == 0) return tempo;
      
      // use local copy
      memcpy(hn_val.bin,blocks+(5*index),5);
      hashname_t id = &hn_val;

      // is it about us?
      link_t link;
      if(hashname_scmp(id,tm->mesh->id) == 0)
      {
        from = tempo->mote;
      }else if((link = mesh_linkid(tm->mesh, id))){
        // about a neighbor
        about = tmesh_moted(tm, link->id); // if we have a link, we may have a mote
      }else{
        // about a neighbor's neighbor, store local copy
        seen = &hn_val;
      }

      index++;
    }
    LOG_CRAZY("about %s from %s seen %s  index %u",about?hashname_short(about->link->id):"NULL",from?hashname_short(from->link->id):"NULL",seen?hashname_short(seen):"NULL",index);

    // loop through blocks for above id/mote
    for(;index < 12;index++)
    {
      mblock_t block = (mblock_t)(blocks+(5*index));
      uint32_t body;
      memcpy(&body,block->body,4);

      LOG_DEBUG("block type %u head %u body %lu",block->type,block->head,body);
      switch(block->type)
      {
        case tmesh_block_end:
          return tempo;
        case tmesh_block_at:
          if(!about) break; // require known mote
          about->signal->at = (body + tempo->at); // is an offset from this tempo
          break;
        case tmesh_block_seq:
          if(!about) break; // require known mote
          about->signal->seq = body;
          break;
        case tmesh_block_qos:
          switch(block->head)
          {
            case 0: // a stream includes qos
              if(tempo->state.is_stream) tempo->qos_remote = body;
              break;
            case 1: // qos request, flag to accept
              if(!from || !tempo->state.is_signal) break; // must be signal to us
              tempo->state.qos_pong = 1; // triggers our signal out
              tempo->qos_remote = body;
              break;
            case 2: // qos accept, clear request
              if(!from || !tempo->state.is_signal) break; // must be signal to us
              tempo->state.qos_ping = tempo->state.qos_pong = 0; // clear all state
              tempo->qos_remote = body;
              // TODO notify app of aliveness?
              break;
            default:
              LOG_WARN("unknown medium block %u: %s",block->head,util_hex((uint8_t*)block,5,NULL));
          }
          break;
        case tmesh_block_app:
          if(about) about->app = body;
          else if(seen && tempo->mote && tm->accept && tm->accept(tm, seen, body))
          {
            LOG_CRAZY("FIXME: routing a link probe to %s via %s",hashname_short(seen),hashname_short(tempo->mote->link->id));
//            mote_route(tempo->mote, seen, hashname_im(tm->mesh->keys, 0x1a));
          }
          break;
        case tmesh_block_medium:
          switch(block->head)
          {
            case 0: // signal medium
              if(about) tempo_medium(about->signal,body);
              break;
            case 1: // stream request
              if(!from) break; // must be to us
              if(!mote_send(from, NULL)) break; // make sure stream exists
              LOG_INFO("stream request from %s on medium %lu",hashname_short(from->link->id),body);
              from->stream->state.requesting = 0; // just in case
              from->stream->state.accepting = 1; // start signalling accept stream
              tempo_medium(from->stream, body);
              break;
            case 2: // stream accept (FUTURE, this is where driver checks resources/acceptibility first)
              if(!from) break; // must be to us
              if(!mote_send(from, NULL)) break; // stream must exist
              LOG_INFO("accepting stream from %s on medium %lu",hashname_short(from->link->id),body);
              from->stream->state.requesting = from->stream->state.accepting = 0; // done signalling
              from->stream->state.direction = 1; // we default to inverted since we're accepting
              from->stream->priority = 3; // little more boost
              from->stream->at = from->signal->at; // same reference sync
              from->stream->seq = from->signal->seq; // TODO invert uint32 for unique starting point
              tempo_medium(from->stream, body);
              break;
            default:
              LOG_WARN("unknown medium block %u: %s",block->head,util_hex((uint8_t*)block,5,NULL));
          }
          if(from) STATED(from->stream);
          break;
        default:
          LOG_WARN("unknown block %u: %s",block->type,util_hex((uint8_t*)block,5,NULL));
      }
    
      // done w/ this mote's blocks
      if(block->done) break;
    }
  }

  return tempo;
}

// breakout for a tx knock on this tempo
static tempo_t tempo_knocked_tx(tempo_t tempo, knock_t knock)
{
  tmesh_t tm = tempo->tm;

  // handle generic error states first
  if(knock->do_err)
  {
    // failed TX, might be bad if too many?
    tempo->c_skip++;
    return LOG_WARN("failed TX, skip at %lu",tempo->c_skip);
  }
  
  // stats
  tempo->c_tx++;

  if(tempo->state.is_signal)
  {

    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){

      // beacon/shared stream reset to when actually sent for sync
      tempo->at = knock->stopped;
      if(tm->stream)
      {
        tm->stream->at = knock->stopped;
      }else if(tempo->mote && tempo->mote->stream){ // sync attached mote stream
        tempo->mote->stream->at = knock->stopped;
        tempo->mote = NULL;
      }else{ // we must always RX once after a beacon
        tempo->priority = 9;
      }
    }else if(tempo == tm->signal){ // shared outgoing signal
      
    }else{ // bad
      return LOG_WARN("invalid signal tx state");
    }
    
  }else{

    util_frames_sent(tempo->frames);
    LOG_CRAZY("tx stream %lu left",util_frames_outlen(tempo->frames));

  }

  return tempo;
}

// breakout for a rx knock on this tempo
static tempo_t tempo_knocked_rx(tempo_t tempo, knock_t knock)
{
  tmesh_t tm = tempo->tm;

  // if error just increment stats and fail
  if(knock->do_err)
  {
    // update fail counters on streams
    if(tempo->state.is_stream)
    {
      if(util_frames_inbox(tempo->frames,NULL,NULL)) tempo->c_miss++;
      else tempo->c_idle++;
    }

    // a signal we are expecting to hear from is a miss
    if(tempo->state.is_signal)
    {
      if(tempo->state.qos_ping) tempo->c_miss++;
      else if(tempo->mote && tempo->mote->stream && tempo->mote->stream->state.requesting) tempo->c_miss++;
      else if(tempo == tm->beacon && !knock->adhoc) tempo_init(tempo); // always reset after non-seek RX fail
      else tempo->c_idle++;
    }

    // shared streams force down with low tolerance for misses (NOTE this logic could be more efficienter)
    if(tempo == tm->stream && !tempo->c_rx && tempo->c_miss > 1)
    {
      LOG_DEBUG("beacon'd stream no response");
      knock->do_gone = 1; 
    }

    return LOG_CRAZY("failed RX, miss %u idle %u",tempo->c_miss,tempo->c_idle);
  }

  uint8_t blocks[60] = {0};
  uint8_t frame[64] = {0};
  uint32_t check = 0;
  mblock_t block;
  if(tempo->state.is_signal)
  {
    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));

    if(tempo == tm->beacon){

      // RX beacon, validate is beacon format
      memcpy(frame,knock->frame,64);
      chacha20(tempo->secret,frame,frame+8,64-8);
      check = murmur4(frame,60);
  
      // beacon encoded signal fail
      if(memcmp(&check,frame+60,4) != 0)
      {
        tempo->c_bad++;
        LOG_CRAZY("%s",util_hex(frame,64,NULL));
        return LOG_INFO("beacon frame invalid, possibly from another community");
      }
      
      // TODO, process these blocks loosely, not fixed
      memcpy(blocks,frame+10,50);
      mote_t from;
      
      // an adhoc signal beacon is in the form of [us][them][stream blocks]
      if(hashname_scmp(hashname_sbin(blocks),tm->mesh->id) == 0 && (from = tmesh_moted(tm,hashname_sbin(blocks+5))))
      {
        LOG_INFO("ad-hoc signal from %s: %s",hashname_short(from->link->id), util_hex(blocks+10,40,NULL));
        memcpy(blocks+5,blocks,5); // adjust format to fit blocks_rx expectations, code sore spot
        tempo_blocks_rx(from->signal,blocks,1); // start offset, also for blocks_rx expectations :/
        // enable an adhoc accept attempt too
        if(from->stream)
        {
          if(from->stream->state.accepting)
          {
            // try one adhoc accept
            from->signal->state.adhoc = 1;
          }else{
            // hack *cough* around the re-use of blocks_rx to override the at sync
            from->stream->at = knock->stopped;
          }
        }
        return from->signal;
      }
      
      // otherwise skip known
      if((from = tmesh_moted(tm,hashname_sbin(blocks)))) return LOG_DEBUG("skipping beacon from known neighbor %s",hashname_short(from->link->id));

      uint8_t index = 1;
      block = (mblock_t)(blocks+(index*5));
      uint32_t medium;
      memcpy(&medium,block->body,4);
      if(!medium) return LOG_WARN("no medium on beacon");
      index++;
      block = (mblock_t)(blocks+(index*5));
      uint32_t seq;
      memcpy(&seq,block->body,4);
      index++;

      // might be busy receiving already
      if(tm->stream)
      {
        if(tm->stream->c_rx && memcmp(blocks,tm->seen,5) != 0) return LOG_INFO("ignoring beacon received from %s while busy with %s",hashname_short(hashname_sbin(blocks)),hashname_short(hashname_sbin(tm->seen)));
        tm->stream = tempo_free(tm->stream);
      }

      // see if driver accepts the new mote
      if(tm->accept && !tm->accept(tm, hashname_sbin(blocks), 0)) return LOG_INFO("driver rejected beacon from %s",hashname_short(hashname_sbin(blocks)));
      memcpy(tm->seen,blocks,5); // copy in sender id to seen to validate later

      LOG_CRAZY("accepted m%lu seq%lu from %s",medium,seq,hashname_short(hashname_sbin(blocks)));
      // sync up beacon for seen accept TX
      tempo->state.seen = 1;
      tempo->medium = medium;
      tempo->seq = seq;
      tempo->at = knock->stopped;

      // see if there's a shared stream accept to us
      uint8_t *to = blocks+(index*5);
      index++;
      block = (mblock_t)(blocks+(index*5));
      if(memcmp(to,hashname_bin(tm->mesh->id),5) == 0 && (block->type == tmesh_block_medium) && (block->head == 2))
      {
        // finish medium/seq
        memcpy(&medium,block->body,4);
        index++;
        block = (mblock_t)(blocks+(index*5));
        uint32_t seq;
        memcpy(&seq,block->body,4);
        index++;

        // new shared stream
        tempo_t stream = tm->stream = tempo_new(tm);
        stream->state.is_stream = 1;
        tempo_init(stream);
        stream->at = knock->stopped;
        stream->seq = seq;
        stream->state.direction = 1; // inverted
        stream->priority = 2;
        tempo_medium(stream, medium);
        STATED(stream);

        // reset beacon
        return tempo_init(tempo);
      }
      
      // leave for next TX
      return tempo;

    }else if(tempo->mote){ // incoming signal for a mote

      // decode/validate signal safely
      memcpy(frame,knock->frame,64);
      chacha20(tempo->secret,knock->nonce,frame,64);
      uint32_t check = murmur4(frame,60);

      // must validate
      if(memcmp(&check,frame+60,4) != 0)
      {
        tempo->c_bad++;
        return LOG_INFO("received signal frame validation failed: %s",util_hex(frame,64,NULL));
      }
      
      // copy frame payload to blocks
      memcpy(blocks,frame,60);

    }else{ // bad
      return LOG_WARN("unknown signal state");
    }
  }else{
    LOG_CRAZY("stream %s",(tempo == tm->stream)?"shared":"private");

    memcpy(frame,knock->frame,64);
    chacha20(tempo->secret,knock->nonce,frame,64);
    LOG_CRAZY("RX data RSSI %d frame %s\n",knock->rssi,util_hex(frame,64,NULL));

    if(!util_frames_inbox(tempo->frames, frame, blocks))
    {
      // if inbox failed but frames still ok, was just mangled bytes
      if(util_frames_ok(tempo->frames))
      {
        tempo->c_bad++;
        return LOG_INFO("bad frame: %s",util_hex(frame,64,NULL));
      }
      knock->do_gone = 1;
      return LOG_INFO("stream broken, flagging gone for full reset");
    }
    
    // app handles processing received stream data
  }

  // update RX flags/stats now
  tempo->c_miss = tempo->c_idle = 0; // clear all rx counters
  tempo->c_rx++; // this will pause mote signal RX while stream is active
  if(knock->rssi > tempo->best || !tempo->best) tempo->best = knock->rssi;
  if(knock->rssi < tempo->worst || !tempo->worst) tempo->worst = knock->rssi;
  tempo->last = knock->rssi;

  // process any blocks
  tempo_blocks_rx(tempo, blocks, 0);

  return tempo;
}

static tempo_t tempo_gone(tempo_t tempo)
{
  tmesh_t tm = tempo->tm;

  if(tempo->state.is_signal)
  {
    LOG_DEBUG("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){
      return LOG_WARN("invalid, our beacon cannot be gone");
    }else if(tempo == tm->signal){ // shared outgoing signal
      return LOG_WARN("invalid, our signal cannot be gone");
    }else if(tempo->mote){ // incoming signal for a mote
      tmesh_demote(tm, tempo->mote);
      return LOG_DEBUG("mote dropped");
    }else{ // bad
      return LOG_WARN("invalid signal state");
    }
  }else{
    LOG_DEBUG("stream %s",(tempo == tm->stream)?"shared":"private");
    if(tempo == tm->stream){ // shared stream
      tm->stream = tempo_free(tempo);
      tempo_init(tm->beacon); // re-initialize beacon
    }else if(tempo->mote){ // private stream
      mote_t mote = tempo->mote;
      // if unsent packets, push into a fresh new stream to start the request process
      if(util_frames_busy(tempo->frames))
      {
        LOG_INFO("copying packets into new stream for %s",hashname_short(mote->link->id));
        mote->stream = NULL;
        lob_t packet;
        while((packet = lob_shift(tempo->frames->outbox)))
        {
          tempo->frames->outbox = packet->next;
          packet->next = NULL;
          mote_send(mote, packet);
        }
        tempo_free(tempo);
      }else{
        mote->stream = tempo_free(tempo);
      }
    }else{ // bad
      return LOG_WARN("invalid stream state");
    }
  }
  
  return LOG_CRAZY("processed gone %p",tempo);
}

// handle a knock that has been sent/received, return mote if packets waiting
tempo_t tmesh_knocked(tmesh_t tm)
{
  if(!tm) return LOG_WARN("bad args");
  knock_t knock = tm->knock;
  tempo_t tempo = knock->tempo;
  
  if(!tempo) return LOG_DEBUG("knock was cancelled");

  // always clear skipped counter and ready flag
  tempo->c_skip = 0;
  knock->is_active = 0;

  tempo_t rxgood = NULL;
  if(knock->is_tx)
  {
    tempo_knocked_tx(tempo, knock);
  }else{
    rxgood = tempo_knocked_rx(tempo, knock);
  }
  
  // if anyone says this tempo is gone, handle it
  if(knock->do_gone) return tempo_gone(tempo);

  // return tempo on successful rx to signal check for full packets waiting 
  return rxgood;
}


// inner logic
static tempo_t tempo_advance(tempo_t tempo, uint32_t at)
{
  if(!tempo || !at) return NULL;
  tmesh_t tm = tempo->tm;

  // initialize to *now* if not set
  if(!tempo->at) tempo->at = at;

  // move ahead window(s)
  while(tempo->at <= at)
  {
    tempo->seq++;
    tempo->c_skip++; // counts missed windows, cleared after knocked

    // use encrypted seed (follows frame)
    uint8_t seed[64+8] = {0};
    uint8_t nonce[8] = {0};
    memcpy(nonce,&(tempo->medium),4);
    memcpy(nonce+4,&(tempo->seq),4);
    chacha20(tempo->secret,nonce,seed,64+8);
    
    // call driver to apply seed to tempo
    if(!tm->advance(tm, tempo, seed+64)) return LOG_WARN("driver advance failed");
  }

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

  LOG_CRAZY("processing for %s at %lu",hashname_short(tm->mesh->id),at);

  // create core tempos first time
  if(!tm->beacon)
  {
    tm->beacon = tempo_new(tm);
    tm->beacon->state.is_signal = 1;
    tempo_init(tm->beacon);
    STATED(tm->beacon);

    tm->signal = tempo_new(tm);
    tm->signal->state.is_signal = 1;
    tempo_init(tm->signal);
    STATED(tm->signal);
  }
  
  // TODO rework how the best is sorted, maybe all at once
  tempo_t best = NULL;
  tempo_advance(tm->beacon, at);
  tempo_advance(tm->stream, at);

  // only schedule beacon or shared stream
  if(!tm->stream) best = tm->sort(tm, best, tm->beacon);
  else best = tm->sort(tm, best, tm->stream);

  // walk all the tempos for next best knock
  mote_t mote;
  bool do_signal = false;
  mote_t adhoc_tx = NULL;
  for(mote=tm->motes;mote;mote=mote->next)
  {
    // convenience
    tempo_t stream = mote->stream;
    tempo_t signal = mote->signal;

    // detect when we're awaiting
    bool awaiting = false;
    if(stream && util_frames_inbox(stream->frames,NULL,NULL)) awaiting = true;

    // advance signal/stream
    tempo_advance(signal, at);
    tempo_advance(stream, at);
    
    // stream is always active if not requesting/accepting
    if(stream && !(stream->state.requesting || stream->state.accepting))
    {
      // any conditions that make us want to wait for a specific type
      do {
        if(stream->state.direction == 1 && !util_frames_outbox(stream->frames,NULL,NULL))
        {
          LOG_DEBUG("stream has nothing to TX, skipping");
          continue;
        }
        // optimize away useless RXs when not awaiting and a flush waiting to TX
        if(stream->state.direction == 0 && stream->frames->flush && !awaiting)
        {
          LOG_DEBUG("skipping stream RX %u, waiting to TX flush",stream->chan);
          continue;
        }
        break;
      } while(tempo_advance(stream,stream->at));

      best = tm->sort(tm, best, stream);
    }
    
    // signal rx is active when qos request, no stream, or stream hasn't worked yet (mirror'd in c_miss counter)
    if(signal->state.qos_ping || !stream  || stream->state.requesting || !stream->c_rx)
    {
      best = tm->sort(tm, best, signal);
    }
    
    // our outgoing signal must be active when:
    if(signal->state.qos_ping || signal->state.qos_pong) do_signal = true;
    if(stream && (stream->state.requesting || stream->state.accepting)) do_signal = true;
    if(signal->state.adhoc) adhoc_tx = mote;
  }

  // upcheck our signal last
  tempo_advance(tm->signal, at);
  if(do_signal) best = tm->sort(tm, best, tm->signal);
  
  STATED(best);
  
  // try a new knock
  knock_t knock = tm->knock;
  memset(knock,0,sizeof(struct knock_struct));

  // first try RX seeking a beacon whenever no shared stream is being established
  if(!tm->stream && !tm->beacon->state.seen)
  {
    knock->tempo = tm->beacon;
    knock->adhoc = best->at;
    
    // try one adhoc signal beacon TX special case
    if(adhoc_tx && !tm->beacon->state.adhoc)
    {
      knock->is_tx = 1;
      tempo_knock_adhoc(adhoc_tx->signal, knock);
      chacha20(knock->tempo->secret,knock->frame,knock->frame+8,64-8);
    }

    // ask driver if it can seek, done if so, else fall through
    knock->is_active = 1;
    if(tm->schedule(tm))
    {
      if(adhoc_tx)
      {
        adhoc_tx->signal->state.adhoc = 0; // only try once
        tm->beacon->state.adhoc = 1; // block others until reset
        tm->beacon->mote = adhoc_tx; // cache to sync sent time in knocked_tx
      }
      return tm;
    }
  }
  
  // proceed w/ normal scheduled knock
  memset(knock,0,sizeof(struct knock_struct));
  
  // copy nonce parts in first
  memcpy(knock->nonce,&(best->medium),4);
  memcpy(knock->nonce+4,&(best->seq),4);
  knock->tempo = best;

  // catch all tx knocks
  if(best->state.is_stream && best->state.direction == 1) knock->is_tx = 1;
  if(best == tm->signal) knock->is_tx = 1;
  if(best == tm->beacon && !best->c_tx) knock->is_tx = 1; // first one is tx, next is rx

  // do the tempo-specific work to fill in the tx frame
  if(knock->is_tx)
  {
    if(!tempo_knock_tx(best, knock)) return LOG_WARN("knock tx prep failed");
    LOG_CRAZY("TX frame %s\n",util_hex(knock->frame,64,NULL));
    if(best == tm->beacon)
    {
      // nonce is prepended to beacons unciphered
      chacha20(best->secret,knock->frame,knock->frame+8,64-8);
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

  LOG_INFO("rebasing for %s at %lu",hashname_short(tm->mesh->id),at);

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
mote_t tmesh_mote(tmesh_t tm, link_t link)
{
  if(!tm || !link) return LOG_WARN("bad args");

  // look for existing and reset stream if given
  mote_t mote;
  for(mote=tm->motes;mote;mote = mote->next) if(mote->link == link) break;

  if(!mote) mote = mote_new(tm, link);

  // only continue if there's a stream to subsume into this mote (NOTE, need to match hashname)
  if(!tm->stream || !tm->stream->c_rx) return mote;

  if(mote->stream) return LOG_WARN("(TODO) cannot replace existing active stream for %s",hashname_short(mote->link->id));

  // bond together
  mote->stream = tm->stream;
  mote->stream->mote = mote;
  tm->stream = NULL; // mote took it over
  tempo_init(tm->beacon); // re-initialize beacon
  
  // ensure link is plumbed for this mote
  link_pipe(link, mote_pipe_send, mote);
  
  // bump priority
  mote->stream->priority = 4;

  STATED(mote->stream);
  STATED(mote->signal);

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

// returns mote for this id if one exists
mote_t tmesh_moted(tmesh_t tm, hashname_t id)
{
  if(!tm || !id) return LOG_WARN("bad args");
  mote_t mote;
  for(mote=tm->motes;mote;mote = mote->next) if(hashname_scmp(mote->link->id,id) == 0) return mote;
  return NULL;
}

// update/signal our current app id
tmesh_t tmesh_appid(tmesh_t tm, uint32_t id)
{
  if(!tm) return LOG_WARN("bad args");
  tm->app = id;

  // make sure we resync all the neighbors
  mote_t mote;
  for(mote=tm->motes;mote;mote=mote->next) mote->signal->state.qos_ping = 1;
  
  return tm;
}

// just the basics ma'am
tmesh_t tmesh_new(mesh_t mesh, char *name, char *pass)
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
