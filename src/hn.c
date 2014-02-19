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
  if(hn->chans) xht_free(hn->chans);
  if(hn->c) crypt_free(hn->c);
  free(hn->paths);
  free(hn);
}

hn_t hn_get(xht_t index, unsigned char *bin)
{
  hn_t hn;
  unsigned char hex[65];
  
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
  util_unhex((unsigned char*)hex,64,bin);
  return hn_get(index,bin);
}

// derive a hn from json in a packet
hn_t hn_getjs(xht_t index, packet_t p)
{
  crypt_t c;
  unsigned char *key;
  hn_t hn = NULL;
  int i, len;
  unsigned short list[64];

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

// load hashname from file
hn_t hn_getfile(xht_t index, char *file)
{
  hn_t id = NULL;
  packet_t p;

  p = util_file2packet(file);
  if(!p) return NULL;
  id = hn_getjs(index, p);
  if(id) return id;
  packet_free(p);
  return NULL;
}

unsigned char hn_distance(hn_t a, hn_t b)
{
  return 0;
}

