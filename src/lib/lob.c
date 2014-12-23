#include "lob.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "util.h" // util_sort(), util_sys_short()
#include "e3x.h" // e3x_rand()
#include "js0n.h"
#include "base32.h"
#include "chacha.h"

lob_t lob_new()
{
  lob_t p;
  if(!(p = malloc(sizeof (struct lob_struct)))) return LOG("OOM");
  memset(p,0,sizeof (struct lob_struct));
  if(!(p->raw = malloc(2))) return lob_free(p);
  memset(p->raw,0,2);
//  LOG("LOB++ %d",p);
  return p;
}

lob_t lob_copy(lob_t p)
{
  lob_t np;
  np = lob_parse(lob_raw(p), lob_len(p));
  return np;
}

lob_t lob_unlink(lob_t parent)
{
  lob_t child;
  if(!parent) return NULL;
  child = parent->chain;
  parent->chain = NULL;
  return child;
}

lob_t lob_link(lob_t parent, lob_t child)
{
  if(!parent) parent = lob_new();
  if(!parent) return NULL;
  if(parent->chain) lob_free(parent->chain);
  parent->chain = child;
  if(child && child->chain == parent) child->chain = NULL;
  return parent;
}

lob_t lob_chain(lob_t p)
{
  lob_t np = lob_new();
  if(!np) return NULL;
  np->chain = p;
  return np;
}

lob_t lob_linked(lob_t parent)
{
  if(!parent) return NULL;
  return parent->chain;
}

// uses given packet like an index, links the child under a name and uses it's ->id, pass NULL to remove/free it
lob_t lob_link_lob(lob_t parent, lob_t child, char *name)
{
  uint32_t id;
  lob_t iter, existing;
  if(!parent || !name) return NULL;
  if(!parent->id) parent->id = 1;

  // check for existing
  if((id = lob_get_uint(parent,name)))
  {
    iter = parent;
    while(iter->chain && iter->chain->id != id) iter = iter->chain;
    existing = iter->chain;
    if(existing && existing->id == id)
    {
      iter->chain = existing->chain;
      existing->chain = NULL;
      lob_free(existing);
    }
  }else{
    // new id
    id = parent->id;
    parent->id++;
    lob_set_uint(parent,name,id);
  }

  // set the id and save the child in the chain
  child->id = id;
  child->chain = parent->chain;
  parent->chain = child;
  return parent;
}

// gets a linked lob from the parent by name
lob_t lob_linked_lob(lob_t parent, char *name)
{
  uint32_t id;
  lob_t iter;
  if(!parent || !name) return NULL;
  if(!(id = lob_get_uint(parent,name))) return NULL;
  for(iter = parent->chain; iter; iter = iter->next) if(iter->id == id) return iter;
  return NULL;
}

lob_t lob_free(lob_t p)
{
  if(!p) return NULL;
//  LOG("LOB-- %d",p);
  if(p->chain) lob_free(p->chain);
  if(p->cache) free(p->cache);
  if(p->raw) free(p->raw);
  free(p);
  return NULL;
}

uint8_t *lob_raw(lob_t p)
{
  if(!p) return NULL;
  return p->raw;
}

size_t lob_len(lob_t p)
{
  if(!p) return 0;
  return 2+p->head_len+p->body_len;
}

lob_t lob_parse(const uint8_t *raw, size_t len)
{
  lob_t p;
  uint16_t nlen, hlen;
  size_t jtest;

  // make sure is at least size valid
  if(!raw || len < 2) return NULL;
  memcpy(&nlen,raw,2);
  hlen = util_sys_short(nlen);
  if(hlen > len-2) return NULL;

  // copy in and update pointers
  p = lob_new();
  if(!(p->raw = realloc(p->raw,len))) return lob_free(p);
  memcpy(p->raw,raw,len);
  p->head_len = hlen;
  p->head = p->raw+2;
  p->body_len = len-(2+p->head_len);
  p->body = p->raw+(2+p->head_len);

  // validate any json
  jtest = 0;
  if(p->head_len >= 2) js0n("\0",1,(char*)p->head,p->head_len,&jtest);
  if(jtest) return lob_free(p);

  return p;
}

uint8_t *lob_head(lob_t p, uint8_t *head, size_t len)
{
  uint16_t nlen;
  void *ptr;
  if(!p) return NULL;

  // new space and update pointers
  if(!(ptr = realloc(p->raw,2+len+p->body_len))) return NULL;
  p->raw = (uint8_t *)ptr;
  p->head = p->raw+2;
  p->body = p->raw+(2+len);
  // move the body forward to make space
  memmove(p->body,p->raw+(2+p->head_len),p->body_len);
  // copy in new head
  memcpy(p->head,head,len);
  p->head_len = len;
  nlen = util_sys_short((uint16_t)len);
  memcpy(p->raw,&nlen,2);
  free(p->cache);
  p->cache = NULL;
  return p->head;
}

uint8_t *lob_body(lob_t p, uint8_t *body, size_t len)
{
  void *ptr;
  if(!p) return NULL;
  if(!(ptr = realloc(p->raw,2+len+p->head_len))) return NULL;
  p->raw = (uint8_t *)ptr;
  p->head = p->raw+2;
  p->body = p->raw+(2+p->head_len);
  if(body) memcpy(p->body,body,len); // allows lob_body(p,NULL,100) to allocate space
  p->body_len = len;
  return p->body;
}

lob_t lob_append(lob_t p, uint8_t *chunk, size_t len)
{
  void *ptr;
  if(!p || !chunk || !len) return LOG("bad args");
  if(!(ptr = realloc(p->raw,2+len+p->body_len+p->head_len))) return NULL;
  p->raw = (unsigned char *)ptr;
  p->head = p->raw+2;
  p->body = p->raw+(2+p->head_len);
  memcpy(p->body+p->body_len,chunk,len);
  p->body_len += len;
  return p;
}

// TODO allow empty val to remove existing
lob_t lob_set_raw(lob_t p, char *key, size_t klen, char *val, size_t vlen)
{
  char *json, *at, *eval;
  size_t len, evlen;

  if(!p || !key || !val) return LOG("bad args (%d,%d,%d)",p,key,val);
  if(p->head_len < 2) lob_head(p, (uint8_t*)"{}", 2);
  // convenience
  if(!klen) klen = strlen(key);
  if(!vlen) vlen = strlen(val);

  // make space and copy
  if(!(json = malloc(klen+vlen+p->head_len+4))) return LOG("OOM");
  memcpy(json,p->head,p->head_len);

  // if it's already set, replace the value
  eval = js0n(key,klen,json,p->head_len,&evlen);
  if(eval)
  {
    // looks ugly, but is just adjusting the space avail for the value to the new size
    // if existing was in quotes, include them
    if(*(eval-1) == '"')
    {
      eval--;
      evlen += 2;
    }
    memmove(eval+vlen,eval+evlen,(json+p->head_len) - (eval+evlen)); // frowney face
    memcpy(eval,val,vlen);
    len = p->head_len - evlen;
    len += vlen;
  }else{
    at = json+(p->head_len-1); // points to the "}"
    // if there's other keys already, add comma
    if(p->head_len >= 7)
    {
      *at = ','; at++;
    }
    *at = '"'; at++;
    memcpy(at,key,klen); at+=klen;
    *at = '"'; at++;
    *at = ':'; at++;
    memcpy(at,val,vlen); at+=vlen;
    *at = '}'; at++;
    len = (size_t)(at - json);
  }
  lob_head(p, (uint8_t*)json, len);
  free(json);
  return p;
}

lob_t lob_set_printf(lob_t p, char *key, const char *format, ...)
{
  va_list ap, cp;
  size_t len;
  char *val;

  if(!p || !key || !format) return LOG("bad args");

  va_start(ap, format);
  va_copy(cp, ap);

  len = (size_t) vsnprintf(NULL, 0, format, cp);
  if((val = malloc(len))) vsprintf(val, format, ap);
  va_end(ap);
  va_end(cp);

  lob_set(p, key, val);
  return p;
}

lob_t lob_set_int(lob_t p, char *key, int val)
{
  char num[32];
  if(!p || !key) return LOG("bad args");
  sprintf(num,"%d",val);
  lob_set_raw(p, key, 0, num, 0);
  return p;
}

lob_t lob_set_uint(lob_t p, char *key, unsigned int val)
{
  char num[32];
  if(!p || !key) return LOG("bad args");
  sprintf(num,"%u",val);
  lob_set_raw(p, key, 0, num, 0);
  return p;
}

lob_t lob_set(lob_t p, char *key, char *val)
{
  char *escaped;
  size_t i, len;
  size_t vlen = strlen(val);
  if(!p || !key || !val) return LOG("bad args");
  // TODO escape key too
  if(!(escaped = malloc(vlen*2+2))) return LOG("OOM"); // enough space worst case
  len = 0;
  escaped[len++] = '"';
  for(i=0;i<vlen;i++)
  {
    if(val[i] == '"' || val[i] == '\\') escaped[len++]='\\';
    escaped[len++]=val[i];
  }
  escaped[len++] = '"';
  lob_set_raw(p, key, 0, escaped, len);
  free(escaped);
  return p;
}

lob_t lob_set_base32(lob_t p, char *key, uint8_t *bin, size_t blen)
{
  char *val;
  if(!p || !key || !bin || !blen) return LOG("bad args");
  size_t vlen = base32_encode_length(blen)-1; // remove the auto-added \0 space
  if(!(val = malloc(vlen+2))) return LOG("OOM"); // include surrounding quotes
  val[0] = '"';
  base32_encode_into(bin, blen, val+1);
  val[vlen+1] = '"';
  lob_set_raw(p,key,0,val,vlen+2);
  return p;
}

// return null-terminated json string
char *lob_json(lob_t p)
{
  if(!p) return NULL;
  if(p->head_len < 2) return NULL;
  if(p->cache) free(p->cache);
  if(!(p->cache = malloc(p->head_len+1))) return LOG("OOM");
  memcpy(p->cache,p->head,p->head_len);
  p->cache[p->head_len] = 0;
  return p->cache;
}


// unescape any json string in place
char *unescape(lob_t p, char *start, size_t len)
{
  char *str, *cursor;

  if(!p || !start || len <= 0) return NULL;

  // make a copy if we haven't yet
  if(!p->cache) lob_json(p);
  if(!p->cache) return NULL;
  
  // switch pointer to the json copy 
  start = p->cache + (start - (char*)p->head);

  // terminate it
  start[len] = 0;

  // unescape it in place in the copy
  for(cursor=str=start; *cursor; cursor++,str++)
  {
    if(*cursor == '\\' && *(cursor+1) == 'n')
    {
      *str = '\n';
      cursor++;
    }else if(*cursor == '\\' && *(cursor+1) == '"'){
      *str = '"';
      cursor++;
    }else{
      *str = *cursor;
    }
  }
  *str = *cursor; // copy null terminator too
  return start;
}

char *lob_get(lob_t p, char *key)
{
  char *val;
  size_t len = 0;
  if(!p || !key || p->head_len < 5) return NULL;
  val = js0n(key,0,(char*)p->head,p->head_len,&len);
  return unescape(p,val,len);
}

char *lob_get_raw(lob_t p, char *key)
{
  char *val;
  size_t len = 0;
  if(!p || !key || p->head_len < 5) return NULL;
  val = js0n(key,0,(char*)p->head,p->head_len,&len);
  if(!val) return NULL;
  // if it's a string value, return start of quotes
  if(*(val-1) == '"') return val-1;
  // everything else is straight up
  return val;
}

size_t lob_get_len(lob_t p, char *key)
{
  char *val;
  size_t len = 0;
  if(!p || !key || p->head_len < 5) return 0;
  val = js0n(key,0,(char*)p->head,p->head_len,&len);
  if(!val) return 0;
  // if it's a string value, include quotes
  if(*(val-1) == '"') return len+2;
  // everything else is straight up
  return len;
}

int lob_get_int(lob_t p, char *key)
{
  char *val = lob_get(p,key);
  if(!val) return 0;
  return (int)strtol(val,NULL,10);
}

unsigned int lob_get_uint(lob_t p, char *key)
{
  char *val = lob_get(p,key);
  if(!val) return 0;
  return (unsigned int)strtoul(val,NULL,10);
}

// returns ["0","1","2"] 
char *lob_get_index(lob_t p, uint32_t i)
{
  char *val;
  size_t len = 0;
  if(!p) return NULL;
  val = js0n(NULL,i,(char*)p->head,p->head_len,&len);
  return unescape(p,val,len);
}

// creates new packet from key:object
lob_t lob_get_json(lob_t p, char *key)
{
  lob_t pp;
  char *val;
  size_t len = 0;
  if(!p || !key) return NULL;

  val = js0n(key,0,(char*)p->head,p->head_len,&len);
  if(!val) return NULL;

  pp = lob_new();
  lob_head(pp, (uint8_t*)val, (uint16_t)len);
  return pp;
}

// list of packet->next from key:[{},{}]
lob_t lob_get_array(lob_t p, char *key)
{
  size_t i;
  char *val;
  size_t len = 0;
  lob_t parr, pent, plast = NULL, pret = NULL;
  if(!p || !key) return NULL;

  parr = lob_get_json(p, key);
  if(!parr || *parr->head != '[')
  {
    lob_free(parr);
    return NULL;
  }

  // parse each object in the array, link together
  for(i=0;(val = js0n(NULL,i,(char*)parr->head,parr->head_len,&len));i++)
  {
    pent = lob_new();
    lob_head(pent, (uint8_t*)val, (uint16_t)len);
    if(!pret) pret = pent;
    else plast->next = pent;
    plast = pent;
  }

  lob_free(parr);
  return pret;
}

// creates new packet w/ a body of the decoded base32 key value
lob_t lob_get_base32(lob_t p, char *key)
{
  lob_t ret;
  char *val;
  size_t len = 0;
  if(!p || !key) return NULL;

  val = js0n(key,0,(char*)p->head,p->head_len,&len);
  if(!val) return NULL;

  ret = lob_new();
  // make space to decode into the body
  if(!lob_body(ret,NULL,base32_decode_length(len))) return lob_free(ret);
  // if the decoding wasn't successful, fail
  if(base32_decode_into(val,len,ret->body) < ret->body_len) return lob_free(ret);
  return ret;
}

// just shorthand for util_cmp to match a key/value
int lob_get_cmp(lob_t p, char *key, char *val)
{
  return util_cmp(lob_get(p,key),val);
}


// count of keys
unsigned int lob_keys(lob_t p)
{
  size_t i, len = 0;
  if(!p) return 0;
  for(i=0;js0n(NULL,i,(char*)p->head,p->head_len,&len);i++);
  if(i % 2) return 0; // must be even number for key:val pairs
  return (unsigned int)i/2;
}

lob_t lob_sort(lob_t p)
{
  unsigned int i, len;
  char **keys;
  lob_t tmp;

  if(!p) return p;
  len = lob_keys(p);
  if(!len) return p;

  // create array of keys to sort
  keys = malloc(len*sizeof(char*));
  if(!keys) return LOG("OOM");
  for(i=0;i<len;i++)
  {
    keys[i] = lob_get_index(p,i*2);
  }

  // use default alpha sort
  util_sort(keys,len,sizeof(char*),NULL,NULL);

  // create the sorted json
  tmp = lob_new();
  for(i=0;i<len;i++)
  {
    lob_set_raw(tmp,keys[i],0,lob_get_raw(p,keys[i]),lob_get_len(p,keys[i]));
  }

  // replace json in original packet
  lob_head(p,tmp->head,tmp->head_len);
  lob_free(tmp);
  free(keys);
  return p;
}


int lob_cmp(lob_t a, lob_t b)
{
  unsigned int i = 0;
  char *str;
  if(!a || !b) return -1;
  if(a->body_len != b->body_len) return -1;
  if(lob_keys(a) != lob_keys(b)) return -1;

  lob_sort(a);
  lob_sort(b);
  while((str = lob_get_index(a,i)))
  {
    if(util_cmp(str,lob_get_index(b,i)) != 0) return -1;
    i++;
  }

  return memcmp(a->body,b->body,a->body_len);
}

lob_t lob_set_json(lob_t p, lob_t json)
{
  char *part;
  uint32_t i = 0;

  while((part = lob_get_index(json,i)))
  {
    lob_set(p,part,lob_get_index(json,i+1));
    i += 2;
  }
  return p;
}

// sha256("telehash")
static const uint8_t _cloak_key[32] = {0xd7, 0xf0, 0xe5, 0x55, 0x54, 0x62, 0x41, 0xb2, 0xa9, 0x44, 0xec, 0xd6, 0xd0, 0xde, 0x66, 0x85, 0x6a, 0xc5, 0x0b, 0x0b, 0xab, 0xa7, 0x6a, 0x6f, 0x5a, 0x47, 0x82, 0x95, 0x6c, 0xa9, 0x45, 0x9a};

// handles cloaking conveniently, len is lob_len()+(8*rounds)
uint8_t *lob_cloak(lob_t p, uint8_t rounds)
{
  uint8_t *ret, *cur, len;
  if(!p || !rounds) return lob_raw(p);
  len = (uint8_t)lob_len(p);
  len += 8*rounds;
  if(!(ret = malloc(len))) return LOG("OOM needed %d",len);
  memset(ret,0,len);
  memcpy(ret+(8*rounds),lob_raw(p),lob_len(p));

  while(rounds)
  {
    cur = ret+(8*(rounds-1));
    e3x_rand(cur, 8);
    if(cur[0] == 0) continue; // re-random until not a 0 byte to start
    rounds--;
    chacha20((uint8_t*)_cloak_key, cur, cur+8, len-((8*rounds)+8));
  }

  return ret;
}

// recursively decloaks and parses
lob_t lob_decloak(uint8_t *cloaked, size_t len)
{
  if(!cloaked || !len) return LOG("bad args");
  if(len < 8 || cloaked[0] == 0) return lob_parse(cloaked, len);
  chacha20((uint8_t*)_cloak_key, cloaked, cloaked+8, (uint32_t)len-8);
  return lob_decloak(cloaked+8, len-8);
}

// linked list utilities

lob_t lob_pop(lob_t list)
{
  lob_t end = list;
  if(!list) return NULL;
  while(end->next) end = end->next;
  end->next = lob_splice(list, end);
  return end;
}

lob_t lob_push(lob_t list, lob_t p)
{
  lob_t end;
  if(!p) return list;
  list = lob_splice(list, p);
  if(!list) return p;
  end = list;
  while(end->next) end = end->next;
  end->next = p;
  p->prev = end;
  return list;
}

lob_t lob_shift(lob_t list)
{
  lob_t start = list;
  list = lob_splice(list, start);
  if(start) start->next = list;
  return start;
}

lob_t lob_unshift(lob_t list, lob_t p)
{
  if(!p) return list;
  list = lob_splice(list, p);
  if(!list) return p;
  p->next = list;
  list->prev = p;
  return p;
}

lob_t lob_splice(lob_t list, lob_t p)
{
  if(!p) return list;
  if(p->next) p->next->prev = p->prev;
  if(p->prev) p->prev->next = p->next;
  if(list == p) list = p->next;
  p->next = p->prev = NULL;
  return list;
}

lob_t lob_insert(lob_t list, lob_t after, lob_t p)
{
  if(!p) return list;
  list = lob_splice(list, p);
  if(!list) return p;
  if(!after) return LOG("bad args, need after");

  p->prev = after;
  p->next = after->next;
  if(after->next) after->next->prev = p;
  after->next = p;
  return list;
}

lob_t lob_freeall(lob_t list)
{
  lob_t next;
  if(!list) return NULL;
  next = list->next;
  lob_free(list);
  return lob_freeall(next);
}
