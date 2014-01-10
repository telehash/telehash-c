#ifndef hn_h
#define hn_h

#include "crypt.h"
#include "xht.h"
#include "path.h"

typedef struct hns_struct
{
  // index of all hashname structures
  xht index;  
} *hns_t;

typedef struct hn_struct
{
  unsigned char hashname[32];
  crypt_t c;
  path_t *paths;
} *hn_t;

// these are functions for all hashnames
hns_t hns_new(int prime);
void hns_free(hns_t h);

// call at least once a minute to clean up old hashnames
void hns_gc(hns_t h);

// fetch/create matching hn (binary or hex)
hn_t hn_get(hns_t h, unsigned char *hn);
hn_t hn_gethex(hns_t h, char *hex);

// load hashname from file
hn_t hn_getfile(hns_t h, char *file);

// load an array of hashnames from a file and return them, caller must free return array
struct hnt_struct *hn_getsfile(hns_t h, char *file);

// return a matching path, or add it if none
path_t hn_path(hn_t hn, path_t p);

unsigned char hn_distance(hn_t a, hn_t b);

#endif