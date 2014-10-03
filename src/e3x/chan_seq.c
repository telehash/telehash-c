#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "chan.h"

typedef struct seq_struct
{
  uint32_t id, nextin, seen, acked;
  lob_t *in;
} *seq_t;

void e3chan_seq_init(e3chan_t c)
{
  seq_t s = malloc(sizeof (struct seq_struct));
  memset(s,0,sizeof (struct seq_struct));
  s->in = malloc(sizeof (lob_t) * c->reliable);
  memset(s->in,0,sizeof (lob_t) * c->reliable);
  c->seq = (void*)s;
}

void e3chan_seq_free(e3chan_t c)
{
  int i;
  seq_t s = (seq_t)c->seq;
  for(i=0;i<c->reliable;i++) lob_free(s->in[i]);
  free(s->in);
  free(s);
}

// add ack, miss to any packet
lob_t e3chan_seq_ack(e3chan_t c, lob_t p)
{
  char *miss;
  int i,max;
  seq_t s = (seq_t)c->seq;

  // determine if we need to ack
  if(!s->nextin) return p;
  if(!p && s->acked && s->acked == s->nextin-1) return NULL;

  if(!p)
  {
    if(!(p = lob_new())) return NULL;
    p->to = c->to;
    lob_set_int(p,"c",c->id);
    DEBUG_PRINTF("making SEQ only");
  }
  s->acked = s->nextin-1;
  lob_set_int(p,"ack",(int)s->acked);

  // check if miss is not needed
  if(s->seen < s->nextin || s->in[0]) return p;
  
  // create miss array, c sux
  max = (c->reliable < 10) ? c->reliable : 10;
  miss = malloc(3+(max*11)); // up to X uint32,'s
  memcpy(miss,"[\0",2);
  for(i=0;i<max;i++) if(!s->in[i]) sprintf(miss+strlen(miss),"%d,",(int)s->nextin+i);
  if(miss[strlen(miss)-1] == ',') miss[strlen(miss)-1] = 0;
  memcpy(miss+strlen(miss),"]\0",2);
  lob_set(p,"miss",miss,strlen(miss));
  free(miss);
  return p;
}

// new channel sequenced packet
lob_t e3chan_seq_packet(e3chan_t c)
{
  lob_t p = lob_new();
  seq_t s = (seq_t)c->seq;
  
  // make sure there's tracking space
  if(e3chan_miss_track(c,s->id,p)) return NULL;

  // set seq and add any acks
  lob_set_int(p,"seq",(int)s->id++);
  return e3chan_seq_ack(c, p);
}

// buffers packets until they're in order
int e3chan_seq_receive(e3chan_t c, lob_t p)
{
  int offset;
  uint32_t id;
  char *seq;
  seq_t s = (seq_t)c->seq;

  // drop or cache incoming packet
  seq = lob_get_str(p,"seq");
  id = seq?(uint32_t)strtol(seq,NULL,10):0;
  offset = id - s->nextin;
  if(!seq || offset < 0 || offset >= c->reliable || s->in[offset])
  {
    lob_free(p);
  }else{
    s->in[offset] = p;
  }

  // track highest seen
  if(id > s->seen) s->seen = id;
  
  return s->in[0] ? 1 : 0;
}

// returns ordered packets for this channel, updates ack
lob_t e3chan_seq_pop(e3chan_t c)
{
  lob_t p;
  seq_t s = (seq_t)c->seq;
  if(!s->in[0]) return NULL;
  // pop off the first, slide any others back, and return
  p = s->in[0];
  memmove(s->in, s->in+1, (sizeof (lob_t)) * (c->reliable - 1));
  s->in[c->reliable-1] = 0;
  s->nextin++;
  return p;
}
