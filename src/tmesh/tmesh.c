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
  tmesh_block_quality, // last known signal quality
  tmesh_block_app // app defined
} tmesh_block_t;

// 5-byte blocks for mesh meta values
typedef struct mblock_struct {
  tmesh_block_t type:3; // medium, at, seq, quality, app
  uint8_t head:4; // per-type
  uint8_t done:1; // last one for cur context
  uint8_t body[4]; // payload
} *mblock_t;

#define STATED(t) util_sys_log(6, "", __LINE__, t->mote?hashname_short(t->mote->link->id):"selfself", \
      "\t\t%s %u %04d %s%u %s %d", \
        t->is_signal?(t->mote?"<-":"->"):"<>", \
        t->medium, t->last, \
        t->do_schedule?"++":"--", \
        t->priority, \
        t->do_request?"R":(t->do_accept?"A":(util_frames_busy(t->frames)?"B":"I")), \
        t->frames?util_frames_outlen(t->frames):-1);


// forward-ho
static tempo_t tempo_new(tmesh_t tm);
static tempo_t tempo_free(tempo_t tempo);
static tempo_t tempo_init(tempo_t tempo, hashname_t id_shared);
static tempo_t tempo_medium(tempo_t tempo, uint32_t medium);
static tempo_t tempo_gone(tempo_t tempo);

// find a stream to send it to for this mote
mote_t mote_send(mote_t mote, lob_t packet)
{
  if(!mote) return LOG_WARN("bad args");
  tempo_t tempo = mote->stream;

  if(!tempo)
  {
    tempo = mote->stream = tempo_new(mote->tm);
    tempo->mote = mote;
    tempo_init(tempo, NULL);
  }

  // if not scheduled, make sure signalling
  if(!tempo->do_schedule) tempo->do_request = 1;
  util_frames_send(tempo->frames, packet);

  LOG_CRAZY("delivering %d to mote %s total %lu",lob_len(packet),hashname_short(mote->link->id),util_frames_outlen(tempo->frames));
  STATED(tempo);

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
  LOG_CRAZY("routing packet to %s via %s",hashname_short(recip->id),hashname_short(mote->link->id));

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
  return LOG_CRAZY("mote free'd");
}

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
  mote->signal->is_signal = 1;
  mote->signal->mote = mote;
  tempo_init(mote->signal, NULL);

  // set up pipe
  if(!(mote->pipe = pipe_new("tmesh"))) return mote_free(mote);
  mote->pipe->arg = mote;
  mote->pipe->send = mote_pipe_send;
  link_pipe(link, mote->pipe);
  
  STATED(mote->signal);

  return mote;
}

static tempo_t tempo_free(tempo_t tempo)
{
  if(!tempo) return NULL;
  tempo->medium = tempo->do_schedule = tempo->priority = 0;
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

  if(!medium || tempo->medium == medium) return tempo;

  // driver init for any medium changes
  uint32_t revert = tempo->medium;
  tempo->medium = medium;
  if(!tempo->tm->init(tempo->tm, tempo))
  {
    tempo->medium = revert;
    LOG_WARN("driver failed medium init from %lu to %lu",revert,medium);
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
static tempo_t tempo_init(tempo_t tempo, hashname_t id_shared)
{
  if(!tempo) return LOG_WARN("bad args");
  tmesh_t tm = tempo->tm;
  uint8_t roll[64] = {0};
  if(memcmp(tempo->secret,roll,32) != 0) return LOG_WARN("tempo already initialized");

  // common base secret from community
  e3x_hash((uint8_t*)(tempo->tm->community),strlen(tempo->tm->community),tempo->secret);
  memcpy(roll,tempo->secret,32);

  // standard breakdown based on tempo type
  if(tempo->is_signal)
  {

    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    tempo_medium(tempo, tm->m_signal);

    if(tempo == tm->beacon)
    {
      tempo->priority = 1; // low
      tempo->do_tx = 1;
      tempo_medium(tempo, tm->m_beacon); // faster starting
      e3x_rand((uint8_t*)&(tempo->seq),4); // random sequence
      // nothing extra to add, just copy for roll
      memcpy(roll+32,roll,32);
    }else if(tempo == tm->signal){ // shared outgoing signal
      tempo->priority = 7; // high
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

    LOG_CRAZY("stream %s",(tempo == tm->stream)?"shared":"private");
    tempo_medium(tempo, tm->m_stream);

    tempo->frames = util_frames_new(64);
    if(tempo == tm->stream) // shared stream
    {
      e3x_rand((uint8_t*)&(tempo->seq),4); // random sequence
      // include given id in the rollup to make shared stream more unique
      memcpy(roll+32,hashname_bin(id_shared),5);
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
  
  STATED(tempo);
  
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

  memset(knock->frame,0,64);
  if(tempo->is_signal)
  {
    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){

      // first is nonce (unciphered prefixed)
      e3x_rand(knock->nonce,8); // random nonce each time
      memcpy(knock->frame,knock->nonce,8);

      // first block is always our short hashname
      memcpy(blocks,hashname_bin(tm->mesh->id),5);

      // then our current app value
      block = (mblock_t)(blocks+5);
      block->type = tmesh_block_app;
      memcpy(block->body,&(tm->app),4);
      
      // init/create new stream
      if(tm->stream)
      {
        LOG_WARN("dropping existing shared stream when sending beacon");
        tm->stream = tempo_free(tm->stream);
      }
      tm->stream = tempo_new(tm);
      tempo_init(tm->stream, tm->mesh->id);

      // then blocks about our shared stream, just medium/seq
      block = (mblock_t)(blocks+5+5);
      block->type = tmesh_block_medium;
      memcpy(block->body,&(tm->stream->medium),4);
      block = (mblock_t)(blocks+5+5+5);
      block->type = tmesh_block_seq;
      memcpy(block->body,&(tm->stream->seq),4);
      
      block->done = 1;

      memcpy(knock->frame+10,blocks,50);

    }else if(tempo == tm->signal){ // shared outgoing signal

      // fill in our outgoing blocks, just cur app value for now
      block = (mblock_t)(blocks);
      block->type = tmesh_block_app;
      memcpy(block->body,&(tm->app),4);

      block->done = 1;

      // fill in blocks for our neighborhood
      // TODO, sort by priority
      for(mote=tm->motes;mote && at < 10;mote = mote->next) // must be at least enough space for two blocks
      {
        // lead w/ short hn
        memcpy(blocks+(++at*5),mote->link->id,5);

        // see if there's a ready stream to advertise
        tempo_t stream = mote->stream;
        if(stream && (stream->do_request || stream->do_accept))
        {
          block = (mblock_t)(blocks+(++at*5));
          if(at >= 12) break;
          block->type = tmesh_block_medium;
      
          // request/accept a bit different
          if(stream->do_request)
          {
            block->head = 1;
          }else{
            block->head = 2;
            // start stream when accept is sent
            stream->do_accept = 0; // accepted
            stream->do_schedule = 1; // start going
            stream->do_tx = 0; // we are inverted
            stream->priority = 3; // little boost
            stream->at = tempo->at;
            stream->seq = tempo->seq; // TODO invert uint32 for unique starting point
            STATED(tempo);
          }

          LOG_CRAZY("signalling %s about a stream %s",hashname_short(mote->link->id),(block->head == 1)?"request":"accept");
          memcpy(block->body,&(stream->medium),4);
        }

        block = (mblock_t)(blocks+(++at*5));
        if(at >= 12) break;
        block->type = tmesh_block_app;
        memcpy(block->body,&(mote->app),4);

        // end this mote's blocks
        block->done = 1;
      }

      memcpy(knock->frame,blocks,60);

    }else if(tempo->mote){ // incoming signal for a mote
    }else{ // bad
      return LOG_WARN("unknown stream state");
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
      block->type = tmesh_block_quality;
      memcpy(block->body,&(tempo->q_local),4);
    
      block = (mblock_t)(blocks+(5+5+5+5));
      block->type = tmesh_block_app;
      memcpy(block->body,&(tm->app),4);

      block->done = 1;

      LOG_INFO("STREAM META >>%s",util_hex(blocks,50,NULL));
      // TODO include signal blocks from last routed-from mote
    }else{ // bad
      return LOG_WARN("unknown stream state");
    }

    // all streams: fill in frame
    if(!util_frames_outbox(tempo->frames,knock->frame,blocks)) return LOG_WARN("frames failed");
    
  }

  return tempo;
}

// process incoming blocks from this tempo
static tempo_t tempo_blocks_rx(tempo_t tempo, uint8_t *blocks)
{
  tmesh_t tm = tempo->tm;
  mote_t about;
  mote_t from; // when about is us
  struct hashname_struct hn_val;
  hashname_t seen;
  uint8_t at;
  if(tempo->mote && !tempo->is_signal) LOG_INFO("STREAM META <<%s",util_hex(blocks,50,NULL));
  for(at = 0;at < 12;at++)
  {
    about = from = NULL;
    seen = NULL;

    // initial about is always sender
    if(!at)
    {
      about = tempo->mote;
    }else{

      // 0's not a valid hn
      uint8_t zeros[5] = {0};
      if(memcmp(zeros,blocks+(5*at),5) == 0) return tempo;
      
      // use local copy
      memcpy(hn_val.bin,blocks+(5*at),5);
      hashname_t id = &hn_val;

      // is it about us?
      link_t link;
      if(hashname_scmp(id,tm->mesh->id) == 0)
      {
        from = tempo->mote;
      }else if((link = mesh_linkid(tm->mesh, id))){
        // about a neighbor
        about = tmesh_mote(tm, link); // if we have a link, we should have a mote
      }else{
        // about a neighbor's neighbor, store local copy
        seen = &hn_val;
      }

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
          return tempo;
        case tmesh_block_at:
          if(!about) break; // require known mote
          about->signal->at = (body + tempo->at); // is an offset from this tempo
          about->signal->do_schedule = 1; // make sure we're listening
          break;
        case tmesh_block_seq:
          if(!about) break; // require known mote
          about->signal->seq = body;
          break;
        case tmesh_block_quality:
          if(about == tempo->mote) tempo->q_remote = body;
          else if(from) from->signal->q_remote = body;
          break;
        case tmesh_block_app:
          if(about) about->app = body;
          else if(seen && tempo->mote && tm->accept && tm->accept(tm, seen, body))
          {
            // TODO mote w/ via
            LOG_WARN("TODO add route to %s via %s",hashname_short(seen),hashname_short(tempo->mote->link->id));
          }
          break;
        case tmesh_block_medium:
          switch(block->head)
          {
            case 0: // signal medium
              if(about) tempo_medium(about->signal,body);
              break;
            case 1: // stream request
              if(!from || !from->link) break; // must be to us
              if(!mote_send(from, NULL)) break; // make sure stream exists
              LOG_CRAZY("stream request from %s on medium %lu",hashname_short(from->link->id),body);
              from->stream->do_accept = 1; // start signalling accept stream
              from->stream->do_schedule = 0; // hold stream scheduling activity
              tempo_medium(from->stream, body);
              break;
            case 2: // stream accept (FUTURE, this is where driver checks resources/acceptibility first)
              if(!from || !from->link) break; // must be to us
              if(!mote_send(from, NULL)) break; // bad juju
              LOG_INFO("accepting stream from %s on medium %lu",hashname_short(from->link->id),body);
              from->stream->do_schedule = 1; // go
              from->stream->do_request = 0; // done signalling
              from->stream->do_tx = 1; // we default to inverted since we're accepting
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

  if(tempo->is_signal)
  {

    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){

      // sync shared stream to when beacon sent
      tm->stream->at = knock->stopped;
      tm->stream->do_schedule = 1;
      tm->stream->do_tx = 1;
      tm->stream->priority = 4;
      STATED(tm->stream);
      
      // quiet beacon while shared stream is active
      tm->beacon->do_schedule = 0;
      STATED(tm->beacon);
      
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
    tempo->c_miss++;
    // shared streams force down with low tolerance for misses (NOTE this logic could be more efficienter)
    if(tempo == tm->stream && !tempo->c_rx && tempo->c_miss > 1)
    {
      LOG_CRAZY("beacon'd stream no response");
      knock->do_gone = 1; 
    }
    // idle mote stream rx fail is a goner
    if(tempo->mote && !tempo->is_signal && !tempo->do_tx && !util_frames_busy(tempo->frames))
    {
      LOG_CRAZY("dropping idle tempo stream");
      knock->do_gone = 1; 
    }
    return LOG_CRAZY("failed RX, miss at %lu",tempo->c_miss);
  }

  // always update RX flags/stats first
  tempo->c_miss = 0; // clear any missed counter
  tempo->c_rx++;
  if(knock->rssi > tempo->best || !tempo->best) tempo->best = knock->rssi;
  if(knock->rssi < tempo->worst || !tempo->worst) tempo->worst = knock->rssi;
  tempo->last = knock->rssi;

  uint8_t blocks[60] = {0};
  uint8_t frame[64] = {0};
  uint32_t check = 0;
  mblock_t block;
  if(tempo->is_signal)
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
        return LOG_CRAZY("beacon received unknown frame: %s",util_hex(frame,64,NULL));
      }
      
      // might be busy receiving already
      if(tm->stream)
      {
        LOG_DEBUG("beacon received while processing another, switching");
        tm->stream = tempo_free(tm->stream);
      }

      // process blocks directly, first sender nickname NOTE: can this be done in tempo_blocks somehow?
      memcpy(blocks,frame+10,50);
      hashname_t id = hashname_sbin(blocks);
      if(mesh_linkid(tm->mesh,id)) LOG_CRAZY("skipping known beacon from %s",hashname_short(id));

      block = (mblock_t)(blocks+5);
      uint32_t app;
      memcpy(&app,block->body,4);
      
      // see if driver accepts the new mote
      if(tm->accept && !tm->accept(tm, id, app)) return LOG_INFO("driver rejected beacon from %s",hashname_short(id));
  
      block = (mblock_t)(blocks+5+5);
      uint32_t medium;
      memcpy(&medium,block->body,4);
      if(!medium) return LOG_WARN("no medium on beacon");

      block = (mblock_t)(blocks+5+5+5);
      uint32_t seq;
      memcpy(&seq,block->body,4);

      // new shared stream
      tempo_t stream = tm->stream = tempo_new(tm);
      tempo_init(stream, hashname_sbin(blocks));
      stream->do_schedule = 1;
      stream->at = knock->stopped;
      stream->seq = seq;
      stream->do_tx = 0; // inverted
      stream->priority = 2;
      tempo_medium(stream, medium);
      STATED(stream);

      // quiet beacon while shared stream is active
      tm->beacon->do_schedule = 0;
      STATED(tm->beacon);
      
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

  // process any blocks
  tempo_blocks_rx(tempo, blocks);
  STATED(tempo);

  return tempo;
}

static tempo_t tempo_gone(tempo_t tempo)
{
  tmesh_t tm = tempo->tm;

  if(tempo->is_signal)
  {
    LOG_CRAZY("signal %s",(tempo == tm->beacon)?"beacon":((tempo == tm->signal)?"out":"in"));
    if(tempo == tm->beacon){
      return LOG_WARN("invalid, our beacon cannot be gone");
    }else if(tempo == tm->signal){ // shared outgoing signal
      return LOG_WARN("invalid, our signal cannot be gone");
    }else if(tempo->mote){ // incoming signal for a mote
      tmesh_demote(tm, tempo->mote);
      return LOG_CRAZY("mote dropped");
    }else{ // bad
      return LOG_WARN("invalid signal state");
    }
  }else{
    LOG_CRAZY("stream %s",(tempo == tm->stream)?"shared":"private");
    if(tempo == tm->stream){ // shared stream
      tm->stream = tempo_free(tempo);
      tm->beacon->do_schedule = 1;
      // use slower beacon medium if connected
      tempo_medium(tm->beacon, tm->motes?tm->m_beacon2:tm->m_beacon);
      STATED(tm->beacon);
    }else if(tempo->mote){ // private stream
      mote_t mote = tempo->mote;
      // idle it
      tempo->do_schedule = 0;
      // if data, re-request, else poof
      if(util_frames_busy(tempo->frames))
      {
        tempo->do_request = 1;
        STATED(tempo);
      }else{
        mote->stream = tempo_free(tempo);
      }
    }else{ // bad
      return LOG_WARN("invalid stream state");
    }
  }
  
  return LOG_CRAZY("processed gone");
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

// handle a knock that has been sent/received, return mote if packets waiting
tempo_t tmesh_knocked(tmesh_t tm)
{
  if(!tm) return LOG_WARN("bad args");
  knock_t knock = tm->knock;
  tempo_t tempo = knock->tempo;

  // always clear skipped counter and ready flag
  tempo->c_skip = 0;
  knock->is_active = 0;

  if(knock->is_tx)
  {
    tempo_knocked_tx(tempo, knock);
  }else{
    tempo_knocked_rx(tempo, knock);
  }
  
  // if anyone says this tempo is gone, handle it
  if(knock->do_gone) return tempo_gone(tempo);

  // return stream with any full packets waiting (NOTE, don't like having to return a raw tempo)
  return (tempo->frames && tempo->frames->inbox)?tempo:NULL;
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
    tm->beacon->is_signal = 1;
    tm->beacon->do_schedule = 1;
    tempo_init(tm->beacon, NULL);
    STATED(tm->beacon);

    tm->signal = tempo_new(tm);
    tm->signal->is_signal = 1;
    tempo_init(tm->signal, NULL);
    STATED(tm->signal);
  }
  
  // start w/ beacon
  tempo_t best = NULL;
  tempo_schedule(tm->beacon, at);
  if(tm->beacon->do_schedule)
  {
    best = tm->sort(tm, best, tm->beacon);
  }
  
  // and shared stream (may not exist)
  if(tm->stream && tm->stream->do_schedule)
  {
    tempo_schedule(tm->stream, at);
    best = tm->sort(tm, best, tm->stream);
  }

  // upcheck our signal
  if(tm->signal->do_schedule)
  {
    tempo_schedule(tm->signal, at);
    best = tm->sort(tm, best, tm->signal);
  }

  // walk all the tempos for next best knock
  mote_t mote;
  for(mote=tm->motes;mote;mote=mote->next)
  {
    // advance signal, always elected for best
    tempo_schedule(mote->signal, at);
    if(mote->signal->do_schedule)
    {
      best = tm->sort(tm, best, mote->signal);
    }
    
    // advance stream too
    if(mote->stream && mote->stream->do_schedule)
    {
      tempo_schedule(mote->stream, at);
      if(mote->stream->do_tx && !util_frames_busy(mote->stream->frames))
      {
        LOG_CRAZY("stream has nothing to send, skipping");
        continue;
      }
      best = tm->sort(tm, best, mote->stream);
    }
  }
  
  // try a new knock
  knock_t knock = tm->knock;
  memset(knock,0,sizeof(struct knock_struct));

  // first try RX seeking a beacon
  knock->tempo = tm->beacon;
  knock->seekto = best->at;

  // ask driver if it can seek, done if so, else fall through
  knock->is_active = 1;
  if(tm->schedule(tm)) return tm;
  
  // proceed w/ normal scheduled knock
  memset(knock,0,sizeof(struct knock_struct));
  
  // copy nonce parts in first
  memcpy(knock->nonce,&(best->medium),4);
  memcpy(knock->nonce+4,&(best->seq),4);
  knock->tempo = best;

  // do the tempo-specific work to fill in the tx frame
  if(best->do_tx)
  {
    knock->is_tx = 1;
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

  // ensure our signal is running
  if(!tm->signal->do_schedule)
  {
    tm->signal->do_schedule = 1;
    STATED(tm->signal);
  }

  // look for existing and reset stream if given
  mote_t mote;
  for(mote=tm->motes;mote;mote = mote->next) if(mote->link == link) break;

  if(!mote) mote = mote_new(tm, link);

  // only continue if there's a stream to subsume into this mote (NOTE, need to match hashname)
  if(!tm->stream || !tm->stream->c_rx) return mote;

  if(mote->stream) LOG_WARN("replacing existing stream for %s",hashname_short(mote->link->id));
  mote->stream = tempo_free(mote->stream);

  // bond together
  mote->stream = tm->stream;
  mote->stream->mote = mote;
  tm->stream = NULL; // mote took it over
  
  // follow w/ handshake
  util_frames_send(mote->stream->frames, link_handshakes(mote->link));

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

// just the basics ma'am
tmesh_t tmesh_new(mesh_t mesh, char *name, char *pass, uint32_t mediums[4])
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
  tm->m_beacon2 = mediums[1];
  tm->m_signal = mediums[2];
  tm->m_stream = mediums[3];

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
