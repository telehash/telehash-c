#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"
#include "pipe.h"

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
  free(p->type);
  if(p->id) free(p->id);
  if(p->path) lob_free(p->path);
  if(p->notify) LOG("pipe free'd leaking notifications");
  free(p);
  return NULL;
}

