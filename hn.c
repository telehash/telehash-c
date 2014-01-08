#include "hn.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "packet.h"
#include "crypt.h"
#include "util.h"
#include "j0g.h"

void hn_init()
{
  _hn_index = xht_new(HNMAXPRIME);
}

void hn_gc()
{
  // xht_walk and look for unused ones
}

hn_t hn_get(unsigned char *bin)
{
  hn_t hn;
  
  hn = xht_get(_hn_index, (const char*)bin);
  if(hn) return hn;

  // init new hashname container
  hn = malloc(sizeof (struct hn_struct));
  bzero(hn,sizeof (struct hn_struct));
  memcpy(hn->hashname, bin, 32);
  xht_set(_hn_index, (const char*)hn->hashname, (void*)hn);
  return hn;
}

hn_t hn_gethex(char *hex)
{
  return 0;
}

// derive a hn from json in a packet
hn_t hn_getjs(packet_t p)
{
  crypt_t c;
  unsigned char *key;
  hn_t hn = NULL;

  // load the crypt from the public key
  key = (unsigned char*)j0g_str("public",(char*)p->json,p->js);
  c = crypt_new(key, strlen((char*)key));
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
  hn = hn_get(crypt_hashname(c));
  if(hn->c) crypt_free(hn->c);
  hn->c = c;
  return hn;
}

// load hashname from file
hn_t hn_getfile(char *file)
{
  hn_t id = NULL;
  packet_t p;

  p = util_file2packet(file);
  if(!p) return NULL;
  id = hn_getjs(p);
  if(id) return id;
  packet_free(p);
  return NULL;
}

// load hashnames from a file and return them, caller must free return array
hn_t *hn_getsfile(char *file)
{
  packet_t p, p2;
  hn_t *list, hn;
  int i, len;

  list = malloc(sizeof (hn_t *));
  bzero(list, sizeof (hn_t *));
  len = 1;
  
  p = util_file2packet(file);
  if(!p) return list;
  if(*p->json != '[')
  {
    packet_free(p);
    return list;
  }

  // parse each object in the array
	for(i=0;p->js[i];i+=2)
	{
    p2 = packet_new();
    packet_json(p2, p->json+p->js[i], p->js[i+1]-p->js[i]);
    hn = hn_getjs(p2);
    packet_free(p2);
    if(!hn) continue;
    list = realloc(list, len * (sizeof (hn_t *)));
    list[len-1] = hn;
    list[len] = 0; // null term
    len++;
	}

  packet_free(p);
  return list;
}
