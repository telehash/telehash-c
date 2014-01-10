#ifndef hnt_h
#define hnt_h

#include "hn.h"

typedef struct hnt_struct
{
  int count;
  hn_t *hns;
} *hnt_t;

hnt_t hnt_new();
void hnt_free(hnt_t t);

void hnt_add(hnt_t t, hn_t h);

// simple array index function
hn_t hnt_get(hnt_t t, int index);


#endif