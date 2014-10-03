#include <string.h>
#include <stdlib.h>
#include "chan.h"

typedef struct miss_struct
{
  uint32_t nextack;
  lob_t *out;
} *miss_t;

void e3chan_miss_init(e3chan_t c)
{
  miss_t m = (miss_t)malloc(sizeof (struct miss_struct));
  memset(m,0,sizeof (struct miss_struct));
  m->out = (lob_t*)malloc(sizeof (lob_t) * c->reliable);
  memset(m->out,0,sizeof (lob_t) * c->reliable);
  c->miss = (void*)m;
}

void e3chan_miss_free(e3chan_t c)
{
  int i;
  miss_t m = (miss_t)c->miss;
  for(i=0;i<c->reliable;i++) lob_free(m->out[i]);
  free(m->out);
  free(m);
}

// 1 when full, backpressure
int e3chan_miss_track(e3chan_t c, uint32_t seq, lob_t p)
{
  miss_t m = (miss_t)c->miss;
  if(seq - m->nextack > (uint32_t)(c->reliable - 1))
  {
    lob_free(p);
    return 1;
  }
  m->out[seq - m->nextack] = p;
  return 0;
}

// looks at incoming miss/ack and resends or frees
void e3chan_miss_check(e3chan_t c, lob_t p)
{
  uint32_t ack;
  int offset, i;
  char *id, *sack;
  lob_t miss = lob_get_packet(p,"miss");
  miss_t m = (miss_t)c->miss;

  sack = lob_get_str(p,"ack");
  if(!sack) return; // grow some
  ack = (uint32_t)strtol(sack, NULL, 10);
  // bad data
  offset = ack - m->nextack;
  if(offset < 0 || offset >= c->reliable) return;

  // free and shift up to the ack
  while(m->nextack <= ack)
  {
    // TODO FIX, refactor check into two stages
//    lob_free(m->out[0]);
    memmove(m->out,m->out+1,(sizeof (lob_t)) * (c->reliable - 1));
    m->out[c->reliable-1] = 0;
    m->nextack++;
  }

  // track any miss packets if we have them and resend
  if(!miss) return;
  for(i=0;(id = lob_get_istr(miss,i));i++)
  {
    ack = (uint32_t)strtol(id,NULL,10);
    offset = ack - m->nextack;
    if(offset >= 0 && offset < c->reliable && m->out[offset]) switch_send(c->s,lob_copy(m->out[offset]));
  }
}

// resends all
void e3chan_miss_resend(e3chan_t c)
{
  int i;
  miss_t m = (miss_t)c->miss;
  for(i=0;i<c->reliable;i++) if(m->out[i]) switch_send(c->s,lob_copy(m->out[i]));
}
