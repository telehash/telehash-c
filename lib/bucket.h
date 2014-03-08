#ifndef bucket_h
#define bucket_h

#include "hn.h"

typedef struct bucket_struct
{
  int count;
  hn_t *hns;
} *bucket_t;

bucket_t bucket_new();
void bucket_free(bucket_t b);

void bucket_add(bucket_t b, hn_t h);

// simple array index function
hn_t bucket_get(bucket_t b, int index);

#endif