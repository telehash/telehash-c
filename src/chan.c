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
  c->type = lob_get(open,"type");

  LOG("new channel %d %s",id,type);
  return c;
}

chan_t chan_free(chan_t c)
{
  if(!c) return NULL;

  // gotta tell handler (TODO, still buggy)
  if(0 && c->state != CHAN_ENDED && c->handle)
  {
    chan_err(c, "closed");
    c->handle(c, c->arg);
  }

  // free any other queued packets
  lob_freeall(c->in);
  free(c);
  return NULL;
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

// process into receiving queue
chan_t chan_receive(chan_t c, lob_t inner)
{
  if(!c || !inner) return LOG("bad args");
  
  c->in = lob_push(c->in, inner);
  return c;
}

// false to force start timers (any new handshake), true to cancel and resend last packet (after any e3x_sync)
chan_t chan_sync(chan_t c, uint8_t sync)
{
  if(!c) return NULL;
  LOG("%d sync %d TODO",c->id,sync);
  return c;
}

// get next avail packet in order, null if nothing
lob_t chan_receiving(chan_t c)
{
  lob_t ret;
  if(!c || !c->in) return NULL;

  ret = lob_shift(c->in);
  c->in = ret->next;
  ret->next = NULL;

  if(lob_get(ret,"end")) c->state = CHAN_ENDED;

  return ret;
}

// outgoing packets

// ack/miss only base packet
lob_t chan_oob(chan_t c)
{
  if(!c) return NULL;

  lob_t ret = lob_new();
  lob_set_uint(ret,"c",c->id);
  
  return ret;
}

// creates a packet w/ necessary json, best way to get valid packet for this channel
lob_t chan_packet(chan_t c)
{
  lob_t ret;
  if(!c) return NULL;
  
  ret = chan_oob(c);

  return ret;
}

// adds to sending queue, expects valid packet
chan_t chan_send(chan_t c, lob_t inner)
{
  if(!c || !inner) return LOG("bad args");
  
  LOG("channel send %d %s",c->id,lob_json(inner));
  if(!c->link)
  {
    lob_free(inner);
    return LOG("dropping packet, no link");
  }

  link_send(c->link, e3x_exchange_send(c->link->x, inner));

  lob_free(inner);

  return c;
}

// generates local-only error packet for next chan_process()
chan_t chan_err(chan_t c, char *msg)
{
  if(!c) return NULL;
  if(!msg) msg = "unknown";
  lob_t err = lob_new();
  lob_set_uint(err,"c",c->id);
  lob_set_raw(err, "end", 3, "true", 4);
  lob_set(err, "err", msg);
  c->in = lob_push(c->in, err); // top of the queue
  return c;
}

// must be called after every send or receive, processes resends/timeouts, fires handlers
chan_t chan_process(chan_t c, uint32_t now)
{
  if(!c) return NULL;

  // do timeout checks
  if(now)
  {
    if(c->timeout)
    {
      // trigger error
      if(now > c->timeout)
      {
        c->timeout = 0;
        chan_err(c, "timeout");
      }else if(c->trecv){
        // kick forward when we have a time difference
        c->timeout += (now - c->trecv);
      }
    }
    c->trecv = now;
  }
  
  // fire receiving handlers
  if(c->in && c->handle) c->handle(c, c->arg);

  if(c->state == CHAN_ENDED)
  {
    LOG("channel is now ended, freeing it");
    c = chan_free(c);
  }
  
  return c;
}

// size (in bytes) of buffered data in or out
uint32_t chan_size(chan_t c)
{
  uint32_t size = 0;
  lob_t cur;
  if(!c) return 0;

  // add up the sizes of the in and out buffers
  cur = c->in;
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
