#include "ext.h"

void ext_peer(chan_t c)
{
  lob_t p;
  hashname_t hn;
  while((p = chan_pop(c)))
  {
    hn = hashname_gethex(c->s->index,lob_get_str(p,"peer"));
    printf("peer HN %s\n",hn?hn->hexname:"null");
    if(!hn)
    {
      lob_free(p);
      continue;
    }
    // TODO send connect
  }
}
