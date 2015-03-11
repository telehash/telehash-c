#include "ext.h"


// new incoming path channel, set up handler
lob_t path_on_open(link_t link, lob_t open)
{
  lob_t path, paths;
  if(!link) return open;
  if(lob_get_cmp(open,"type","path")) return open;
  
  LOG("incoming path ping %s",lob_json(open));
  
  // load all incoming paths into link
  for(paths = path = lob_get_array(open,"paths");path;path = lob_next(path)) link_path(link,path);
  lob_free(paths);

  // respond on all known pipes
  // link_pipes iter

  return NULL;
}

/* TODO migrate upwards

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
*/