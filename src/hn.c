#include "hn.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "packet.h"
#include "crypt.h"
#include "util.h"
#include "j0g.h"
#include "js0n.h"
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
  char *part, csids[16], csid[3]; // max parts of 8
  int i,ids;
  unsigned char *hall, hnbin[32];

  if(!p) return NULL;

  for(ids=i=0;ids<8 && p->js[i];i+=4)
  {
    memcpy(csids+ids,p->json+p->js[i],2);
    ids++;
  }
  
  if(!ids) return NULL;
  
  qsort(csids,ids,2,csidcmp);

  hall = malloc(ids*2*32);
  for(i=0;i<ids;i++)
  {
    memcpy(csid,csids+(i*2),2);
    csid[2] = 0;
    part = packet_get_str(p, csid);
    if(!part) continue; // garbage safety
    crypt_hash(csid,2,hall+(i*2*32));
    crypt_hash(part,strlen(part),hall+(((i*2)+1)*32));
  }
  crypt_hash(hall,ids*2*32,hnbin);
  free(hall);
  hn = hn_get(index, hnbin);
  if(!hn) return NULL;
  hn->parts = p;
  return hn;
}

hn_t hn_frompacket(xht_t index, packet_t p)
{
  return NULL;
}

// derive a hn from json format
hn_t hn_fromjson(xht_t index, packet_t p)
{
  crypt_t c;
  unsigned char *key;
  hn_t hn = NULL;
  int i, len;
  unsigned short list[64];
  char csid;
  packet_t parts;

  if(!p) return NULL;
  
  // generate the hashname
  parts = packet_get_packet(p, "parts");
  if(!parts) return NULL;
  hn = hn_getparts(index, parts);
  
  // get the matching csid
  csid = hn_csid(index,hn);

  // load the crypt from the public key
  key = (unsigned char*)j0g_str("public",(char*)p->json,p->js);
  c = crypt_new(0x2a, key, strlen((char*)key));
  if(!c) return NULL;
  
  // if there's a private key, try loading that too, only used in loading id.json and such
  if(j0g_val("private",(char*)p->json,p->js))
  {
    key = (unsigned char*)j0g_str("private",(char*)p->json,p->js);
    if(crypt_private(c,key,strlen((char*)key)))
    {
      crypt_free(c);
      return NULL;
    }
  }

  // get/update our hn value
  hn = hn_get(index, crypt_hashname(c));
  if(hn->c) crypt_free(hn->c);
  hn->c = c;
  
  // if any paths are stored, associte them
  i = j0g_val("paths",(char*)p->json,p->js);
  if(i)
  {
    key = p->json+p->js[i];
    len = p->js[i+1];
    js0n(key, len, list, 64);
  	for(i=0;list[i];i+=2)
  	{
      path_t path;
      path = hn_path(hn, path_parse(key+list[i], list[i+1]));
      if(path) path->atIn = 0; // don't consider this path alive
  	}
    
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

char hn_csid(xht_t index, hn_t hn)
{
  int i=0;
  char *csid, best=0, id;
  if(!hn || !hn->parts) return 0;
  while((csid = packet_get_istr(hn->parts,i)))
  {
    util_unhex(csid,2,&id);
    if(strlen(csid) == 2 && id > best && xht_get(index,csid)) best = id;
    i+=2;
  }
  return best;
}

unsigned char hn_distance(hn_t a, hn_t b)
{
  return 0;
}

