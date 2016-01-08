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
  pipe_notify(p, 1);
  free(p->type);
  if(p->id) free(p->id);
  if(p->path) lob_free(p->path);
  free(p);
  return NULL;
}

pipe_t pipe_notify(pipe_t p, uint8_t down)
{
  if(!p) return NULL;
  lob_t list;
  for(list=p->links;list;list = lob_next(list))
  {
    link_t link = list->arg;
    if(!down)
    {
      link_resync(link);
      continue;
    }
    // TODO remove pipe from link, if no pipes mark link down
  }
  return p;
}
