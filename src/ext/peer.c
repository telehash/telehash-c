#include "ext.h"

void ext_peer(chan_t c)
{
  packet_t p;
  hn_t hn;
  while((p = chan_pop(c)))
  {
    hn = hn_gethex(c->s->index,packet_get_str(p,"peer"));
    printf("peer HN %s\n",hn?hn->hexname:"null");
    if(!hn)
    {
      packet_free(p);
      continue;
    }
    // TODO send connect
  }
}
