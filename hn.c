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

// load hashname from file
hn_t hn_idfile(char *file)
{
  hn_t id = NULL;
  packet_t p;
  crypt_t c;
  unsigned char *key;

  p = util_file2packet(file);
  if(!p) return id;
  
  // load the crypt from the public key
  key = (unsigned char*)j0g_str("public",(char*)p->json,p->js);
  c = crypt_new(key, strlen((char*)key));
  if(!c)
  {
    packet_free(p);
    return id;
  }
  
  // if there's a private key, try loading that too
  if(j0g_val("private",(char*)p->json,p->js))
  {
    key = (unsigned char*)j0g_str("private",(char*)p->json,p->js);
    if(crypt_private(c,key,strlen((char*)key)))
    {
      packet_free(p);
      crypt_free(c);
      return id;
    }
  }
  
  // get/update our hn value
  id = hn_get(crypt_hashname(c));
  if(id->c) crypt_free(id->c);
  id->c = c;
  return id;
}

// load hashnames from a seeds file and return them, caller must free return array
hn_t *hn_seeds(char *file)
{
  hn_t *seeds;
  seeds = malloc(sizeof (hn_t *));
  bzero(seeds, sizeof (hn_t *));
  return seeds;
}
