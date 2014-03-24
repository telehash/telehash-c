#include <string.h>
#include <stdlib.h>
#include "switch.h"

typedef struct seq_struct
{
  uint32_t seq;
  packet_t in;
} *seq_t;

void chan_seq_init(chan_t c)
{
  seq_t s = malloc(sizeof (struct seq_struct));
  memset(s,0,sizeof (struct seq_struct));
  c->seq = (void*)s;
}

void chan_seq_free(chan_t c)
{
  // TODO free any waiting packets
  free(c->seq);
}

// adds seq, ack, miss
void chan_seq_add(chan_t c, packet_t p)
{
  
}

// buffers packets until they're in order
void chan_seq_receive(chan_t c, packet_t p)
{
  
}

// returns ordered packets for this channel, updates ack
packet_t chan_seq_pop(chan_t c)
{
  return NULL;
}
