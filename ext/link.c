#include "ext.h"

void ext_link(chan_t c)
{
  packet_t p;
  while((p = chan_pop(c)))
  {
    printf("link packet %.*s\n", p->json_len, p->json);      
    packet_free(p);
  }
}