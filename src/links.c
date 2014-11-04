#include "links.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "lib/lib.h"

links_t links_new()
{
  links_t b = malloc(sizeof (struct links_struct));
  memset(b,0,sizeof (struct links_struct));
  b->links = NULL;
  b->args = NULL;
  return b;
}

void links_free(links_t b)
{
  if(!b) return;
  if(b->links) free(b->links);
  if(b->args) free(b->args);
  free(b);
}

/*
void links_add(links_t b, hashname_t hn)
{
  void *ptr;
  if(!b || !hn) return;
  // already here?
  if(links_in(b,hn) >= 0) return;
  // make more space, args too if any
  if(b->args)
  {
    if(!(ptr = realloc(b->args, (b->count+1) * sizeof(void*)))) return;
    b->args = (void**)ptr;
    b->args[b->count] = NULL;
  }
  if(!(ptr = realloc(b->hns, (b->count+1) * sizeof(hashname_t)))) return;
  b->hns = (hashname_t*)ptr;
  b->hns[b->count] = hn;
  b->count++;
}

void links_rem(links_t b, hashname_t hn)
{
  int i, at = -1;
  if(!b || !hn) return;
  for(i=0;i<b->count;i++) if(b->hns[i] == hn) at = i;
  if(at == -1) return;
  b->count--;
  memmove(b->hns+at,b->hns+(at+1),b->count-at);
  if(b->args) memmove(b->args+at,b->args+(at+1),b->count-at);
}

hashname_t links_get(links_t b, int index)
{
  if(!b || index >= b->count) return NULL;
  return b->hns[index];
}

int links_in(links_t b, hashname_t hn)
{
  int i;
  if(!b || !hn) return -1;
  for(i=0;i<b->count;i++) if(b->hns[i] == hn) return i;
  return -1;
}

// these set and return an optional arg for the matching hashname
void links_set(links_t b, hashname_t hn, void *arg)
{
  int i;
  links_add(b,hn);
  i = links_in(b,hn);
  if(i < 0) return;
  if(!b->args)
  {
    b->args = malloc(b->count * sizeof(void*));
    if(!b->args) return;
    memset(b->args,0,b->count * sizeof (void*));
  }
  b->args[i] = arg;
}

void *links_arg(links_t b, hashname_t hn)
{
  int i = links_in(b,hn);
  if(i < 0 || !b->args) return NULL;
  return b->args[i];
}

*/