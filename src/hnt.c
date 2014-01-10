#include "hnt.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
  
hnt_t hnt_new()
{
  hnt_t t = malloc(sizeof (struct hnt_struct));
  bzero(t,sizeof (struct hnt_struct));
  t->hns = malloc(sizeof (hn_t));
  t->hns[0] = NULL;
  return t;
}

void hnt_free(hnt_t t)
{
  free(t->hns);
  free(t);
}

void hnt_add(hnt_t t, hn_t h)
{
  int i;
  for(i=0;t->hns[i];i++) if(t->hns[i] == h) return;
  t->hns = realloc(t->hns, (i+2) * sizeof(hn_t));
  t->hns[i] = h;
  t->hns[i+1] = NULL;
  t->count = i+1;
}

hn_t hnt_get(hnt_t t, int index)
{
  if(index >= t->count) return NULL;
  return t->hns[index];
}