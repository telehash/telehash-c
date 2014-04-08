#include "ext.h"

void ext_path(chan_t c)
{
  packet_t p;
  while((p = chan_pop(c)))
  {
    DEBUG_PRINTF("TODO path packet %.*s\n", p->json_len, p->json);      
    packet_free(p);
  }
}