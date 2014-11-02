#include "ext.h"

void path_sync(switch_t s, hashname_t hn)
{
  // TODO use s->id->paths to send ours to hn, gather results to update best hn->out
  // set up handler for responses
}

void ext_path(chan_t c)
{
  lob_t p, pp, next;
  path_t path;
  while((p = chan_pop(c)))
  {
    DEBUG_PRINTF("TODO path packet %.*s\n", p->json_len, p->json);
    pp = lob_get_packets(p, "paths");
    while(pp)
    {
      path = hashname_path(c->to, path_parse((char*)pp->json, pp->json_len),0);
      if(path && (next = chan_packet(c)))
      {
        next->out = path;
        lob_set(next,"path",path_json(path),0);
        chan_send(c,next);
      }
      next = pp->next;
      lob_free(pp);
      pp = next;
    }
    lob_free(p);
  }
  chan_end(c, NULL);
}