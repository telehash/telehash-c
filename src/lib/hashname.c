#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "telehash.h"
#include "telehash.h"
#include "telehash.h" // for sha256 e3x_hash()

// how many csids can be used to make a hashname
#define MAX_CSIDS 8

struct hashname_struct
{
  uint8_t bin[32];
};

// v* methods return this
static struct hashname_struct hn_vtmp;

hashname_t hashname_dup(hashname_t id)
{
  hashname_t hn;
  if(!(hn = malloc(sizeof (struct hashname_struct)))) return NULL;
  memset(hn,0,sizeof (struct hashname_struct));
  if(id) memcpy(hn->bin, id->bin, 32);
  return hn;
}

hashname_t hashname_free(hashname_t hn)
{
  if(hn == &hn_vtmp) return LOG("programmer error");
  if(hn) free(hn);
  return NULL;
}

// validate a str is a base32 hashname, returns TEMPORARY hashname
hashname_t hashname_vchar(const char *str)
{
  if(!str) return NULL;
  if(strlen(str) != 52) return NULL;
  if(base32_decode(str,52,hn_vtmp.bin,32) != 32) return NULL;
  return &hn_vtmp;
}

hashname_t hashname_vbin(const uint8_t *bin)
{
  if(!bin) return NULL;
  memcpy(&hn_vtmp,bin,32);
  return &hn_vtmp;
}

// temp hashname from intermediate values as hex/base32 key/value pairs
hashname_t hashname_vkey(lob_t key, uint8_t csid)
{
  unsigned int i, start;
  uint8_t hash[64];
  char *id, *value, hexid[3];
  if(!key) return LOG("invalid args");
  util_hex(&csid, 1, hexid);

  // get in sorted order
  lob_sort(key);

  // loop through all keys rolling up
  for(i=0;(id = lob_get_index(key,i));i+=2)
  {
    value = lob_get_index(key,i+1);
    if(strlen(id) != 2 || !util_ishex(id,2) || !value) continue; // skip non-id keys
    
    // hash the id
    util_unhex(id,2,hash+32);
    start = (i == 0) ? 32 : 0; // only first one excludes previous rollup
    e3x_hash(hash+start,(32-start)+1,hash); // hash in place

    // get the value from the body if matching csid arg
    if(util_cmp(id, hexid) == 0)
    {
      if(key->body_len == 0) return LOG("missing key body");
      // hash the body
      e3x_hash(key->body,key->body_len,hash+32);
    }else{
      if(strlen(value) != 52) return LOG("invalid value %s %d",value,strlen(value));
      if(base32_decode(value,52,hash+32,32) != 32) return LOG("base32 decode failed %s",value);
    }
    e3x_hash(hash,64,hash);
  }
  if(!i || i % 2 != 0) return LOG("invalid keys %d",i);
  
  return hashname_vbin(hash);
}

hashname_t hashname_vkeys(lob_t keys)
{
  hashname_t hn;
  lob_t im;

  if(!keys) return LOG("bad args");
  im = hashname_im(keys,0);
  hn = hashname_vkey(im,0);
  lob_free(im);
  return hn;
}

// accessors
uint8_t *hashname_bin(hashname_t hn)
{
  if(!hn) return NULL;
  return hn->bin;
}

// 52 byte base32 string w/ \0 (TEMPORARY)
static char hn_ctmp[53];
char *hashname_char(hashname_t hn)
{
  if(!hn) return NULL;
  base32_encode(hn->bin,32,hn_ctmp,53);
  return hn_ctmp;
}

// 16 byte base32 string w/ \0 (TEMPORARY)
char *hashname_short(hashname_t hn)
{
  static uint8_t tog = 1;
  if(!hn) return NULL;
  tog = tog ? 0 : 26; // fit two short names in hn_ctmp for easier LOG() args
  base32_encode(hn->bin,10,hn_ctmp+tog,53-tog);
  return hn_ctmp+tog;
}

int hashname_cmp(hashname_t a, hashname_t b)
{
  if(!a || !b) return -1;
  return memcmp(a->bin,b->bin,32);
}

uint8_t hashname_id(lob_t a, lob_t b)
{
  uint8_t id, best;
  uint32_t i;
  char *key;

  if(!a || !b) return 0;

  best = 0;
  for(i=0;(key = lob_get_index(a,i));i+=2)
  {
    if(strlen(key) != 2) continue;
    if(!lob_get(b,key)) continue;
    id = 0;
    util_unhex(key,2,&id);
    if(id > best) best = id;
  }
  
  return best;
}

// intermediate hashes in the json, if id is given it is attached as BODY instead
lob_t hashname_im(lob_t keys, uint8_t id)
{
  uint32_t i;
  size_t len;
  uint8_t *buf, hash[32];
  char *key, *value, hex[3];
  lob_t im;

  if(!keys) return LOG("bad args");

  // loop through all keys and create intermediates
  im = lob_new();
  buf = NULL;
  util_hex(&id,1,hex);
  for(i=0;(key = lob_get_index(keys,i));i+=2)
  {
    value = lob_get_index(keys,i+1);
    if(strlen(key) != 2 || !value) continue; // skip non-csid keys
    len = base32_decode_floor(strlen(value));
    // save to body raw or as a base32 intermediate value
    if(id && util_cmp(hex,key) == 0)
    {
      lob_body(im,NULL,len);
      if(base32_decode(value,strlen(value),im->body,len) != len) continue;
      lob_set_raw(im,key,0,"true",4);
    }else{
      buf = util_reallocf(buf,len);
      if(!buf) return lob_free(im);
      if(base32_decode(value,strlen(value),buf,len) != len) continue;
      // store the hash intermediate value
      e3x_hash(buf,len,hash);
      lob_set_base32(im,key,hash,32);
    }
  }
  if(buf) free(buf);
  return im;
}


