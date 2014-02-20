#include "hn.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "packet.h"
#include "crypt.h"
#include "util.h"
#include "chan.h"

void hn_free(hn_t hn)
{
  if(!hn) return;
  if(hn->chans) xht_free(hn->chans);
  if(hn->c) crypt_free(hn->c);
  if(hn->parts) packet_free(hn->parts);
  free(hn->paths);
  free(hn);
}

hn_t hn_get(xht_t index, unsigned char *bin)
{
  hn_t hn;
  unsigned char hex[65];
  
  if(!bin) return NULL;
  util_hex(bin,32,hex);
  hn = xht_get(index, (const char*)hex);
  if(hn) return hn;

  // init new hashname container
  hn = malloc(sizeof (struct hn_struct));
  bzero(hn,sizeof (struct hn_struct));
  memcpy(hn->hashname, bin, 32);
  memcpy(hn->hexname, hex, 65);
  xht_set(index, (const char*)hn->hexname, (void*)hn);
  hn->paths = malloc(sizeof (path_t));
  hn->paths[0] = NULL;
  return hn;
}

hn_t hn_gethex(xht_t index, char *hex)
{
  unsigned char bin[32];
  if(!hex) return NULL;
  util_unhex((unsigned char*)hex,64,bin);
  return hn_get(index,bin);
}

int csidcmp(const void *a, const void *b)
{
  if(*(char*)a == *(char*)b) return *(char*)(a+1) - *(char*)(b+1);
  return *(char*)a - *(char*)b;
}

hn_t hn_getparts(xht_t index, packet_t p)
{
  char *part, csid, csids[16], hex[3]; // max parts of 8
  int i,ids;
  unsigned char *hall, hnbin[32];
  char best = 0;
  hn_t hn;

  if(!p) return NULL;
  hex[2] = 0;

  for(ids=i=0;ids<8 && p->js[i];i+=4)
  {
    if(p->js[i+1] != 2) continue; // csid must be 2 char only
    memcpy(hex,p->json+p->js[i],2);
    memcpy(csids+ids,hex,2);
    util_unhex((unsigned char*)hex,2,(unsigned char*)&csid);
    if(csid > best && xht_get(index,hex)) best = csid; // matches if we have the same csid in index (for our own keys)
    ids++;
  }
  
  if(!best) return NULL; // we must match at least one
  
  qsort(csids,ids,2,csidcmp);

  hall = malloc(ids*2*32);
  for(i=0;i<ids;i++)
  {
    memcpy(hex,csids+(i*2),2);
    part = packet_get_str(p, hex);
    if(!part) continue; // garbage safety
    crypt_hash((unsigned char*)hex,2,hall+(i*2*32));
    crypt_hash((unsigned char*)part,strlen(part),hall+(((i*2)+1)*32));
  }
  crypt_hash(hall,ids*2*32,hnbin);
  free(hall);
  hn = hn_get(index, hnbin);
  if(!hn) return NULL;

  if(!hn->parts) hn->parts = p;
  else packet_free(p);
  
  hn->csid = best;
  util_hex((unsigned char*)&best,1,(unsigned char*)hn->hexid);

  return hn;
}

hn_t hn_frompacket(xht_t index, packet_t p)
{
  hn_t hn = NULL;
  if(!p) return NULL;
  
  // get/gen the hashname
  hn = hn_getparts(index, packet_get_packet(p, "from"));
  if(!hn) return NULL;

  // load key from packet body
  if(!hn->c && p->body)
  {
    hn->c = crypt_new(hn->csid, p->body, p->body_len);
    if(!hn->c) return NULL;
  }

  return hn;
}

// derive a hn from json format
hn_t hn_fromjson(xht_t index, packet_t p)
{
  char *key;
  hn_t hn = NULL;
  packet_t pp, next;
  path_t path;

  if(!p) return NULL;
  
  // get/gen the hashname
  hn = hn_getparts(index, packet_get_packet(p, "parts"));
  if(!hn) return NULL;

  // load the matching public key
  pp = packet_get_packet(p, "keys");
  if(!hn->c && pp)
  {
    key = packet_get_str(pp,hn->hexid);
    if(!key) return NULL;
    hn->c = crypt_new(hn->csid, (unsigned char*)key, strlen(key));
    if(!hn->c) return NULL;
  }
  packet_free(pp);
  
  // if any paths are stored, associte them
  pp = packet_get_packets(p, "paths");
  while(pp)
  {
    path = hn_path(hn, path_parse(pp->json, pp->json_len));
    if(path) path->atIn = 0; // don't consider this path alive
    next = pp->next;
    packet_free(pp);
    pp = next;
  }
  
  return hn;
}

path_t hn_path(hn_t hn, path_t p)
{
  path_t ret = NULL;
  int i;

  if(!p) return NULL;

  // find existing matching path
  for(i=0;hn->paths[i];i++)
  {
    if(path_match(hn->paths[i], p)) ret = hn->paths[i];
  }
  if(!ret)
  {
    // add new path, i is the end of the list from above
    hn->paths = realloc(hn->paths, (i+2) * (sizeof (path_t)));
    hn->paths[i] = path_parse(path_json(p),0); // create a new copy
    hn->paths[i+1] = 0; // null term
    ret = hn->paths[i];
  }

  // update state tracking
  hn->last = ret;
  ret->atIn = time(0);

  return ret;
}

unsigned char hn_distance(hn_t a, hn_t b)
{
  return 0;
}

