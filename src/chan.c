#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "telehash.h"

// open must be chan_receive or chan_send next yet
chan_t chan_new(lob_t open)
{
  uint32_t id;
  char *type;
  chan_t c;

  if(!open) return LOG("open packet required");
  id = (uint32_t)lob_get_int(open,"c");
  if(!id) return LOG("invalid channel id");
  type = lob_get(open,"type");
  if(!type) return LOG("missing channel type");

  c = malloc(sizeof (struct chan_struct));
  memset(c,0,sizeof (struct chan_struct));
  c->state = CHAN_OPENING;
  c->id = id;
  c->open = lob_copy(open);
  c->type = lob_get(open,"type");
  c->capacity = 1024*1024; // 1MB total default

  // reliability
  if(lob_get(open,"seq"))
  {
    c->seq = 1;
  }

  LOG("new %s channel %d %s",c->seq?"reliable":"unreliable",id,type);
  return c;
}

void chan_free(chan_t c)
{
  if(!c) return;
  // free cached packet
  lob_free(c->open);
  // free any other queued packets
  lob_freeall(c->in);
  lob_freeall(c->out);
  // TODO signal handler
  // TODO remove from link->chans
  free(c);
}

// return the numeric per-exchange id
uint32_t chan_id(chan_t c)
{
  if(!c) return 0;
  return c->id;
}

// this will set the default inactivity timeout using this event timer and our uid
uint32_t chan_timeout(chan_t c, uint32_t at)
{
  if(!c) return 0;

  // no timeout, just return how much time is left
  if(!at) return c->timeout;

  c->timeout = at;
  return c->timeout;
}

// returns the open packet (always cached)
lob_t chan_open(chan_t c)
{
  if(!c) return NULL;
  return c->open;
}

chan_t chan_next(chan_t c)
{
  if(!c) return NULL;
  return c->next;
}

enum chan_states chan_state(chan_t c)
{
  if(!c) return CHAN_ENDED;
  return c->state;
}

// incoming packets

// usually sets/updates event timer, ret if accepted/valid into receiving queue
uint8_t chan_receive(chan_t c, lob_t inner, uint32_t now)
{
  lob_t prev, miss;
  uint32_t ack;

  if(!c) return 1;
  
  // do timeout checks
  if(now)
  {
    if(c->timeout)
    {
      // trigger error
      if(now > c->timeout)
      {
        c->timeout = 0;
        // even if there was a packet, it's invalid now
        lob_free(inner);
        inner = lob_new();
        lob_set_uint(inner,"c",c->id);
        lob_set(inner, "err", "timeout");
      }else if(c->trecv){
        // kick forward when we have a time difference
        c->timeout += (now - c->trecv);
      }
    }
    c->trecv = now;
  }

  // now we need a packet
  if(!inner) return 1;
  
  // no reliability, just append and done
  if(!c->seq)
  {
    c->in = lob_push(c->in, inner);
    return 0;
  }

  // reliability
  inner->id = lob_get_uint(inner, "seq");

  // if there's an id, it's content
  if(inner->id)
  {
    if(inner->id <= c->acked)
    {
      LOG("dropping old seq %d",inner->id);
      return 1;
    }

    // insert it sorted
    prev = c->in;
    while(prev && prev->id > inner->id) prev = prev->next;
    if(prev && prev->id == inner->id)
    {
      LOG("dropping dup seq %d",inner->id);
      lob_free(inner);
      return 1;
    }
    c->in = lob_insert(c->in, prev, inner);
//    LOG("inserted seq %d",c->in?c->in->id:-1);
  }

  // remove any from outgoing cache that have been ack'd
  if((ack = lob_get_uint(inner, "ack")))
  {
    prev = c->out;
    while(prev && prev->id <= ack)
    {
      c->out = lob_splice(c->out, prev);
      lob_free(prev); // TODO, notify app this packet was ack'd
      prev = c->out;
    }

    // process array, update list of packets that are missed or not
    if((miss = lob_get_json(inner, "miss")))
    {
      // set resend flag timestamp on them
      // update window
    }

  }

  if(c->handle) c->handle(c, c->arg);

  return 0;
}

// false to force start timers (any new handshake), true to cancel and resend last packet (after any e3x_sync)
void chan_sync(chan_t c, uint8_t sync)
{
  if(!c) return;
  LOG("%d sync %d TODO",c->id,sync);
}

// get next avail packet in order, null if nothing
lob_t chan_receiving(chan_t c)
{
  lob_t ret;
  if(!c || !c->in) return NULL;

  // reliable must be sorted, catch when there's a gap
  if(c->seq && c->in->id != c->ack + 1)
  {
    c->acked = c->ack - 1; // this forces an ack+miss on the next sending
    return NULL;
  }

  ret = lob_shift(c->in);
  c->in = ret->next;
  c->ack = ret->id; // track highest processed

  return ret;
}

// outgoing packets

// ack/miss only base packet
lob_t chan_oob(chan_t c)
{
  lob_t ret, cur;
  char *miss = NULL;
  uint32_t seq, last, delta;
  size_t len;
  if(!c) return NULL;

  ret = lob_new();
  lob_set_uint(ret,"c",c->id);
  
  if(!c->seq) return ret;
  
  // check for ack/miss
  if(c->ack != c->acked)
  {
    lob_set_uint(ret,"ack",c->ack);

    // also check to include misses
    cur = c->in;
    last = c->ack;
    if(cur && (cur->id - last) != 1)
    {
      // I'm so tired of strings in c
      len = 2;
      if(!(miss = malloc(len))) return lob_free(ret);
      len = (size_t)snprintf(miss,len,"[");
      for(seq=c->ack+1; cur; seq++)
      {
//        LOG("ack %d seq %d last %d cur %d",c->ack,seq,last,cur->id);
        // if we have this seq, skip to next packet
        if(cur->id <= seq)
        {
          cur = cur->next;
          continue;
        }
        // insert this missing seq delta
        delta = seq - last;
        last = seq;
        len += (size_t)snprintf(NULL, 0, "%" PRIu32 ",", delta) + 1;
        if(!(miss = util_reallocf(miss, len))) return lob_free(ret);
        sprintf(miss+strlen(miss),"%" PRIu32 ",", delta);
      }
      // add current window at the end
      delta = 100; // TODO calculate this from actual space avail
      len += (size_t)snprintf(NULL, 0, "%" PRIu32 "]", delta) + 1;
      if(!(miss = util_reallocf(miss, len))) return lob_free(ret);
      sprintf(miss+strlen(miss),"%" PRIu32 "]", delta);
      lob_set_raw(ret,"miss",4,miss,strlen(miss));
    }

    c->acked = c->ack;
  }
  
  free(miss);
  return ret;
}

// creates a packet w/ necessary json, best way to get valid packet for this channel
lob_t chan_packet(chan_t c)
{
  lob_t ret;
  if(!c) return NULL;
  
  ret = chan_oob(c);

  // if reliable, add seq here too
  if(c->seq)
  {
    ret->id = c->seq; // use the lob id for convenience/checks
    lob_set_uint(ret,"seq",c->seq++);
  }

  return ret;
}

// adds to sending queue, expects valid packet
uint8_t chan_send(chan_t c, lob_t inner)
{
  if(!c || !inner) return 1;
  
  LOG("channel send %d %s",c->id,lob_json(inner));
  c->out = lob_push(c->out, inner);

  return 0;
}

// must be called after every send or receive, pass pkt to e3x_encrypt before sending
lob_t chan_sending(chan_t c, uint32_t now)
{
  lob_t ret;
  if(!c) return NULL;
  
  // packets waiting
  if(c->out)
  {
    ret = lob_shift(c->out);
    c->out = ret->next;
    // if it's a content packet and reliable, add to sent list
    if(c->seq && ret->id) c->sent = lob_push(c->sent, lob_copy(ret));
    return ret;
  }
  
  // check sent list for any flagged to resend
  for(ret = c->sent; ret; ret = ret->next)
  {
    if(!ret->id) continue;
    if(ret->id == now) continue;
    // max once per second throttle resend
    ret->id = now;
    return ret;
  }
  
  LOG("sending ack %d acked %d",c->ack,c->acked);

  // if we need to generate an ack/miss yet, do that
  if(c->ack != c->acked) return chan_oob(c);
  
  return NULL;
}

// size (in bytes) of buffered data in or out
uint32_t chan_size(chan_t c, uint32_t max)
{
  uint32_t size = 0;
  lob_t cur;
  if(!c) return 0;

  // update max capacity if given
  if(max) c->capacity = max;

  // add up the sizes of the in and out buffers
  cur = c->in;
  while(cur)
  {
    size += lob_len(cur);
    cur = cur->next;
  }
  cur = c->out;
  while(cur)
  {
    size += lob_len(cur);
    cur = cur->next;
  }

  return size;
}

// set up internal handler for all incoming packets on this channel
chan_t chan_handle(chan_t c, void (*handle)(chan_t c, void *arg), void *arg)
{
  if(!c) return LOG("bad args");

  c->handle = handle;
  c->arg = arg;

  return c;
}
