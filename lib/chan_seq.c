#include <string.h>
#include <stdlib.h>
#include "switch.h"

typedef struct seq_struct
{
  uint16_t seq;
  packet_t *inp;
  uint32_t *ins;
  int in;
} *seq_t;

void chan_seq_init(chan_t c)
{
  seq_t s = malloc(sizeof (struct seq_struct));
  memset(s,0,sizeof (struct seq_struct));
  s->inp = malloc(sizeof (packet_t) * c->reliable);
  s->ins = malloc(sizeof (uint32_t) * c->reliable);
  c->seq = (void*)s;
}

void chan_seq_free(chan_t c)
{
  int i=0;
  seq_t s = (seq_t)c->seq;
  for(i=0;i<s->in;s++) packet_free(s->inp[i]);
  free(s->inp);
  free(s->ins);
  free(s);
}

// adds seq, ack, miss
void chan_seq_add(chan_t c, packet_t p)
{
//  seq_t s = (seq_t)c->seq;
  
}

// buffers packets until they're in order
void chan_seq_receive(chan_t c, packet_t p)
{
//  seq_t s = (seq_t)c->seq;
  
}

// returns ordered packets for this channel, updates ack
packet_t chan_seq_pop(chan_t c)
{
//  seq_t s = (seq_t)c->seq;
  return NULL;
}
