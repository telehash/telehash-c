#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "telehash.h"

pipe_t pipe_new(char *type)
{
  pipe_t p;
  if(!type) return LOG("no type");

  if(!(p = malloc(sizeof (struct pipe_struct)))) return NULL;
  memset(p,0,sizeof (struct pipe_struct));
  p->type = strdup(type);
  return p;
}

pipe_t pipe_free(pipe_t p)
{
  if(!p) return NULL;
  p->send = NULL;
  pipe_changed(p);
  if(p->links) LOG("leaking link notifications");
  free(p->type);
  if(p->id) free(p->id);
  if(p->path) lob_free(p->path);
  free(p);
  return NULL;
}

pipe_t pipe_changed(pipe_t p)
{
  if(!p) return NULL;
  lob_t list;
  for(list=p->links;list;list = lob_next(list))
  {
    link_t link = list->arg;
    link_pipe(link, p); // will remove pipe if it's down
    link_resync(link);
  }
  return p;
}

// safe wrapper around ->send
pipe_t pipe_send(pipe_t pipe, lob_t packet, link_t link)
{
  if(!pipe || !packet) return NULL;
  if(!pipe->send)
  {
    LOG("dropping packet to down pipe %s: %s",pipe->id, lob_json(packet));
    lob_free(packet);
    return pipe;
  }
  pipe->send(pipe, packet, link);
  return pipe;
}
