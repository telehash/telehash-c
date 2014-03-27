#include "ext.h"
#include "switch.h"

void ext_thtp(chan_t c)
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