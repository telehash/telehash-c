#ifndef links_h
#define links_h

#include "hashname.h"

// simple array of hashname management

typedef struct links_struct
{
  int count;
  hashname_t *hns;
  void **args;
} *links_t;

links_t links_new();
void links_free(links_t b);

// adds/removes hashname
void links_add(links_t b, hashname_t h);
void links_rem(links_t b, hashname_t h);

// returns index position if in the bucket, otherwise -1
int links_in(links_t b, hashname_t h);

// these set and return an optional arg for the matching hashname
void links_set(links_t b, hashname_t h, void *arg);
void *links_arg(links_t b, hashname_t h);

// simple array index function
hashname_t links_get(links_t b, int index);

#endif