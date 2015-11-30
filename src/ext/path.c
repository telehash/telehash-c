#include <string.h>
#include "telehash.h"

// list of active pipes and state per link
typedef struct ping_struct
{
  uint64_t at;
  void (*pong)(link_t link, lob_t status, void *arg);
  void *arg;
} *ping_t;

// handle incoming packets for the built-in stream channel
void path_ping_handler(chan_t chan, void *arg)
{
  lob_t packet;
  ping_t ping = (ping_t)arg;
  if(!chan || !ping) return;

  while((packet = chan_receiving(chan)))
  {
    LOG("response pong %s",lob_json(packet));
    if(ping->pong)
    {
      lob_set_uint(packet,"rtt",util_since(ping->at));
      ping->pong(chan->link, packet, ping->arg);
      ping->pong = NULL;
    }
    // TODO process incoming paths for public IPs
    
    lob_free(packet);
  }
  
  if(chan_state(chan) == CHAN_ENDED) free(ping);
}

// send a path ping and get callback event
link_t path_ping(link_t link, void (*pong)(link_t link, lob_t status, void *arg), void *arg)
{
  chan_t chan;
  lob_t open;
  ping_t ping;

  // ping tracker obj
  if(!(ping = malloc(sizeof (struct ping_struct)))) return LOG("oom");
  memset(ping,0,sizeof (struct ping_struct));
  ping->pong = pong;
  ping->arg = arg;
  ping->at = util_at();

  open = lob_new();
  lob_set(open,"type","path");
  // paths

  // create new channel, set it up, then receive this open
  chan = link_chan(link, open);
  chan_handle(chan, path_ping_handler, ping);
  chan_send(chan, open);
  return link;
}

// new incoming path channel, set up handler
lob_t path_on_open(link_t link, lob_t open)
{
  lob_t path, tmp;
  pipe_t pipe;
  if(!link) return open;
  if(lob_get_cmp(open,"type","path")) return open;
  
  LOG("incoming path ping %s",lob_json(open));
  
  // load all incoming paths into link
  for(tmp = path = lob_get_array(open,"paths");path;path = lob_next(path)) link_path(link,path);
  lob_freeall(tmp);

  // respond on all known pipes
  tmp = lob_new();
  lob_set_uint(tmp,"c",lob_get_uint(open,"c"));
  for(pipe = link_pipes(link,NULL); pipe; pipe = link_pipes(link,pipe))
  {
    if(pipe->path) lob_set_raw(tmp,"path",0,(char*)pipe->path->head,pipe->path->head_len);
    else lob_set_raw(tmp,"path",0,"{}",0);
    link_direct(link, lob_copy(tmp), pipe);
  }
  lob_free(tmp);

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