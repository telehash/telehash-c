#ifndef links_h
#define links_h

#include "link.h"

// simple array of links management

typedef struct links_struct
{
  int count;
  link_t *links;
  void **args;
} *links_t;

links_t links_new();
void links_free(links_t b);

// adds/removes link
void links_add(links_t b, link_t link);
void links_rem(links_t b, link_t link);

// returns index position if in the list, otherwise -1
int links_in(links_t b, link_t link);

// these set and return an optional arg for the matching link
void links_set(links_t b, link_t link, void *arg);
void *links_arg(links_t b, link_t link);

// simple array index function
link_t links_get(links_t b, int index);

#endif