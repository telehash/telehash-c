#include "path.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

path_t path_new(char *type)
{
  path_t p = malloc(sizeof (struct path_struct));
  bzero(p,sizeof (struct path_struct));
  memcpy(p->type,type,strlen(type)+1);
  p->json = malloc(1);
  bzero(p->json,1);
  return p;
}

void path_free(path_t p)
{
  free(p->json);
  free(p);
}
