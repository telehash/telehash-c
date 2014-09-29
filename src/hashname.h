#ifndef hn_h
#define hn_h

#include "crypt.h"
#include "path.h"
#include "xht.h"
#include "packet.h"

typedef struct hn_struct
{
  unsigned char hashname[32];
  char csid, hexid[3], hexname[65]; // for convenience
  unsigned long chanOut, chanMax; // for channel id tracking
  crypt_t c;
  path_t *paths, last;
  xht_t chans;
  packet_t parts;
} *hn_t;

// fetch/create matching hn (binary or hex)
hn_t hn_get(xht_t index, unsigned char *hn);
hn_t hn_gethex(xht_t index, char *hex);

// loads from raw parts object, sets ->parts and ->csid
hn_t hn_getparts(xht_t index, struct packet_struct *p);

// derive a hn from packet ("from" and body), loads matching CSID
hn_t hn_frompacket(xht_t index, struct packet_struct *p);

// derive a hn from json format
hn_t hn_fromjson(xht_t index, struct packet_struct *p);

// return a matching path, or add it if none, valid=1 updates hn->last and path->tin
path_t hn_path(hn_t hn, path_t p, int flag);

unsigned char hn_distance(hn_t a, hn_t b);

#endif