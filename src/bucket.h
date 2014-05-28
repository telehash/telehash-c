#ifndef bucket_h
#define bucket_h

#include "hn.h"

// simple array of hashname management

typedef struct bucket_struct
{
  int count;
  hn_t *hns;
  void **args;
} *bucket_t;

bucket_t bucket_new();
void bucket_free(bucket_t b);

// adds/removes hashname
void bucket_add(bucket_t b, hn_t h);
void bucket_rem(bucket_t b, hn_t h);

// returns index position if in the bucket, otherwise -1
int bucket_in(bucket_t b, hn_t h);

// these set and return an optional arg for the matching hashname
void bucket_set(bucket_t b, hn_t h, void *arg);
void *bucket_arg(bucket_t b, hn_t h);

// simple array index function
hn_t bucket_get(bucket_t b, int index);

#endif