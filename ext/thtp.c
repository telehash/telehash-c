#include "ext.h"

void ext_thtp(chan_t c)
{
  packet_t p;
  while((p = chan_pop(c)))
  {
    printf("thtp packet %.*s\n", p->json_len, p->json);      
    packet_free(p);
  }
}