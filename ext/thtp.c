#include "ext.h"
#include "switch.h"
#include <string.h>
#include <stdlib.h>

struct thtp_struct 
{
  xht_t x;
};

thtp_t thtp_new()
{
  thtp_t t;
  t = malloc(sizeof (struct thtp_struct));
  memset(t,0,sizeof (struct thtp_struct));
  return t;
}

thtp_t thtp_free(thtp_t t)
{
  if(!t) return t;
  free(t);
  return NULL;
}

thtp_t thtp_xht(thtp_t t, xht_t index)
{
  if(!t) return t;
  t->x = index;
  return t;
}

void ext_thtp(thtp_t t, chan_t c)
{
  packet_t p, thp;
  while((p = chan_pop(c)))
  {
    printf("thtp packet %.*s\n", p->json_len, p->json);      
    packet_free(p);
    p = chan_packet(c);
    packet_set(p,"end","true",4);
    thp = packet_new();
    packet_set_int(thp,"status",200);
    packet_body(thp,(unsigned char*)"ok",2);
    packet_body(p,packet_raw(thp),packet_len(thp));
    packet_free(thp);
    switch_send(c->s,p);
  }
}