#ifndef hashname_h
#define hashname_h

/*
#include "crypt.h"
#include "path.h"
#include "xht.h"
#include "lob.h"

typedef struct hashname_struct
{
  unsigned char hashname[32];
  char csid, hexid[3], hexname[65]; // for convenience
  unsigned long chanOut, chanMax; // for channel id tracking
  crypt_t c;
  path_t *paths, last;
  xht_t chans;
  lob_t parts;
} *hashname_t;

// fetch/create matching hn (binary or hex)
hashname_t hashname_get(xht_t index, unsigned char *hn);
hashname_t hashname_gethex(xht_t index, char *hex);

// loads from raw parts object, sets ->parts and ->csid
hashname_t hashname_getparts(xht_t index, struct lob_struct *p);

// derive a hn from packet ("from" and body), loads matching CSID
hashname_t hashname_frompacket(xht_t index, struct lob_struct *p);

// derive a hn from json format
hashname_t hashname_fromjson(xht_t index, struct lob_struct *p);

// return a matching path, or add it if none, valid=1 updates hn->last and path->tin
path_t hashname_path(hashname_t hn, path_t p, int flag);

unsigned char hashname_distance(hashname_t a, hashname_t b);

*/

#endif