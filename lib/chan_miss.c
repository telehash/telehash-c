#include <string.h>
#include <stdlib.h>
#include "switch.h"

typedef struct miss_struct
{
  packet_t *outp;
  uint32_t *outs;
  int out;
} *miss_t;

void chan_miss_init(chan_t c)
{
  miss_t m = (miss_t)malloc(sizeof (struct miss_struct));
  memset(m,0,sizeof (struct miss_struct));
  m->outp = (packet_t*)malloc(sizeof (packet_t) * c->reliable);
  m->outs = (uint32_t*)malloc(sizeof (uint32_t) * c->reliable);
  c->miss = (void*)m;
}

void chan_miss_free(chan_t c)
{
  miss_t m = (miss_t)c->miss;
  // TODO free every out packet
  free(m->outp);
  free(m->outs);
  free(m);
}

// null when full
packet_t chan_miss_packet(chan_t c)
{
  miss_t m = (miss_t)c->miss;
  if(m->out == c->reliable) return NULL;
  return packet_new();
}

// buffers packets to be able to re-send
void chan_miss_send(chan_t c, packet_t p)
{
  miss_t m = (miss_t)c->miss;
  // should never happen but just to be safe
  if(m->out == c->reliable) return;
  // make a copy into out
}

// looks at incoming miss/ack and resends or frees
void chan_miss_check(chan_t c, packet_t p)
{
//  miss_t m = (miss_t)c->miss;
  // loop through out
  // free any older than ack
  // resend any in miss
}
