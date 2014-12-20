#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include "e3x.h"

// every new channel has a unique global id
static uint32_t _uids = 0;

// internal only structure, always use accessors
struct e3x_channel_struct
{
  uint32_t id; // wire id (not unique)
  char c[12]; // str of id
  char uid[9]; // process hex id (unique)
  char *type;
  lob_t open; // cached for convenience
  enum e3x_channel_states state;
  uint32_t capacity, max; // totals for windowing

  // timer stuff
  uint32_t tsent, trecv; // last send, recv from util_sys_seconds
  uint32_t tsince, timeout; // tsince=start, timeout is how many seconds before auto-err
  lob_t timer; // the timer that has been sent to ev
  e3x_event_t ev; // the event manager to update our timer with
  
  // reliable tracking
  lob_t out, sent;
  lob_t in;
  uint32_t seq, ack, acked, window;
};

// open must be e3x_channel_receive or e3x_channel_send next yet
e3x_channel_t e3x_channel_new(lob_t open)
{
  uint32_t id;
  char *type;
  e3x_channel_t c;

  if(!open) return LOG("open packet required");
  id = (uint32_t)lob_get_int(open,"c");
  if(!id) return LOG("invalid channel id");
  type = lob_get(open,"type");
  if(!type) return LOG("missing channel type");

  c = malloc(sizeof (struct e3x_channel_struct));
  memset(c,0,sizeof (struct e3x_channel_struct));
  c->state = OPENING;
  c->id = id;
  sprintf(c->c,"%u",id);
  c->open = lob_copy(open);
  c->type = lob_get(open,"type");
  c->capacity = 1024*1024; // 1MB total default

  // generate a unique id in hex
  _uids++;
  util_hex((uint8_t*)&_uids,4,c->uid);

  // reliability
  if(lob_get(open,"seq"))
  {
    c->seq = 1;
  }

  LOG("new %s channel %s %d %s",c->seq?"reliable":"unreliable",c->uid,id,type);
  return c;
}

void e3x_channel_free(e3x_channel_t c)
{
  if(!c) return;
  // cancel timeouts
  e3x_channel_timeout(c,NULL,0);
  // free cached packet
  lob_free(c->open);
  // free any other queued packets
  lob_freeall(c->in);
  lob_freeall(c->out);
  free(c);
}

// return its unique id in hex
char *e3x_channel_uid(e3x_channel_t c)
{
  if(!c) return NULL;
  return c->uid;
}

// return the numeric per-exchange id
uint32_t e3x_channel_id(e3x_channel_t c)
{
  if(!c) return 0;
  return c->id;
}

char *e3x_channel_c(e3x_channel_t c)
{
  if(!c) return NULL;
  return c->c;
}

// internally calculate until timeout
uint32_t _time_left(e3x_channel_t c)
{
  uint32_t at = util_sys_seconds();
  if(!c) return 0;
  if(!c->timeout) return 1; // never timeout
  // some seconds left yet
  if((at - c->tsince) < c->timeout) return c->timeout - (at - c->tsince);
  return 0;
}

// this will set the default inactivity timeout using this event timer and our uid
uint32_t e3x_channel_timeout(e3x_channel_t c, e3x_event_t ev, uint32_t timeout)
{
  if(!c) return 0;

  // un-set any
  if(ev != c->ev)
  {
    // cancel and clearn any previous timer state
    c->timeout = 0;
    if(c->ev) e3x_event_set(c->ev,NULL,c->uid,0);
    c->ev = NULL;
    lob_free(c->timer);
    c->timer = NULL;
  }
  
  // no event manager, no timeouts
  if(!ev) return 0;
  
  // no timeout, just return how much time is left
  if(!timeout) return _time_left(c);

  // add/update new timeout
  c->tsince = util_sys_seconds(); // start timer now
  c->timeout = timeout;
  c->ev = ev;
  c->timer = lob_new();
  lob_set_uint(c->timer,"c",c->id);
  lob_set(c->timer, "id", c->uid);
  lob_set(c->timer, "err", "timeout");
  e3x_event_set(c->ev, c->timer, lob_get(c->timer, "id"), timeout*1000); // ms in the future
  return _time_left(c);
}

// returns the open packet (always cached)
lob_t e3x_channel_open(e3x_channel_t c)
{
  if(!c) return NULL;
  return c->open;
}

enum e3x_channel_states e3x_channel_state(e3x_channel_t c)
{
  if(!c) return ENDED;
  return c->state;
}

// incoming packets

// usually sets/updates event timer, ret if accepted/valid into receiving queue
uint8_t e3x_channel_receive(e3x_channel_t c, lob_t inner)
{
  lob_t prev, miss;
  uint32_t ack;

  if(!c || !inner) return 1;

  // TODO timer checks
  if(inner == c->timer)
  {
    // uhoh or resend
    return 1;
  }
  
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

  // TODO reset timer stuff

  return 0;
}

// false to force start timers (any new handshake), true to cancel and resend last packet (after any e3x_sync)
void e3x_channel_sync(e3x_channel_t c, uint8_t sync)
{
  if(!c) return;
  LOG("%s sync %d TODO",c->uid,sync);
}

// get next avail packet in order, null if nothing
lob_t e3x_channel_receiving(e3x_channel_t c)
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
lob_t e3x_channel_oob(e3x_channel_t c)
{
  lob_t ret, cur;
  char *miss;
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
        len += (size_t)snprintf(NULL, 0, "%u,", delta) + 1;
        if(!(miss = realloc(miss, len))) return lob_free(ret);
        sprintf(miss+strlen(miss),"%u,", delta);
      }
      // add current window at the end
      delta = 100; // TODO calculate this from actual space avail
      len += (size_t)snprintf(NULL, 0, "%u]", delta) + 1;
      if(!(miss = realloc(miss, len))) return lob_free(ret);
      sprintf(miss+strlen(miss),"%u]", delta);
      lob_set_raw(ret,"miss",4,miss,strlen(miss));
    }

    c->acked = c->ack;
  }
  
  return ret;
}

// creates a packet w/ necessary json, best way to get valid packet for this channel
lob_t e3x_channel_packet(e3x_channel_t c)
{
  lob_t ret;
  if(!c) return NULL;
  
  ret = e3x_channel_oob(c);

  // if reliable, add seq here too
  if(c->seq)
  {
    ret->id = c->seq; // use the lob id for convenience/checks
    lob_set_uint(ret,"seq",c->seq++);
  }

  return ret;
}

// adds to sending queue, expects valid packet
uint8_t e3x_channel_send(e3x_channel_t c, lob_t inner)
{
  if(!c || !inner) return 1;
  
  LOG("channel send %d %s",c->id,lob_json(inner));
  c->out = lob_push(c->out, inner);

  return 0;
}

// must be called after every send or receive, pass pkt to e3x_encrypt before sending
lob_t e3x_channel_sending(e3x_channel_t c)
{
  lob_t ret;
  uint32_t now;
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
  now = util_sys_seconds();
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
  if(c->ack != c->acked) return e3x_channel_oob(c);
  
  return NULL;
}

// size (in bytes) of buffered data in or out
uint32_t e3x_channel_size(e3x_channel_t c, uint32_t max)
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
