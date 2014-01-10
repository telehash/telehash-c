#ifndef hn_h
#define hn_h

#include "crypt.h"
#include "path.h"
#include "xht.h"

typedef struct hn_struct
{
  unsigned char hashname[32];
  char hexname[65]; // for convenience
  crypt_t c;
  path_t *paths;
} *hn_t;

// fetch/create matching hn (binary or hex)
hn_t hn_get(xht_t index, unsigned char *hn);
hn_t hn_gethex(xht_t index, char *hex);

// load hashname from file
hn_t hn_getfile(xht_t index, char *file);

// load an array of hashnames from a file and return them, caller must free return array
struct hnt_struct *hn_getsfile(xht_t index, char *file);

// return a matching path, or add it if none
path_t hn_path(hn_t hn, path_t p);

unsigned char hn_distance(hn_t a, hn_t b);

#endif