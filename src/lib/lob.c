#include "lob.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "js0n.h"
#include "platform.h" // platform_short()
#include "util.h" // util_sort()

lob_t lob_new()
{
  lob_t p;
  if(!(p = malloc(sizeof (struct lob_struct)))) return NULL;
  memset(p,0,sizeof (struct lob_struct));
  if(!(p->raw = malloc(2))) return lob_free(p);
  memset(p->raw,0,2);
  p->quota = 1440; // default MTU
//  DEBUG_PRINTF("packet +++ %d",p);
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

lob_t lob_free(lob_t p)
{
  if(!p) return NULL;
//  DEBUG_PRINTF("packet --- %d",p);
  if(p->chain) lob_free(p->chain);
  if(p->json) free(p->json);
  if(p->raw) free(p->raw);
  free(p);
  return NULL;
}

uint16_t lob_space(lob_t p)
{
  uint16_t len;
  if(!p) return 0;
  len = lob_len(p);
  if(len > p->quota) return 0;
  return p->quota-len;
}

uint8_t *lob_raw(lob_t p)
{
  if(!p) return NULL;
  return p->raw;
}

uint32_t lob_len(lob_t p)
{
  if(!p) return 0;
  return 2+p->head_len+p->body_len;
}

lob_t lob_parse(uint8_t *raw, uint32_t len)
{
  lob_t p;
  uint16_t nlen, hlen;
  int jtest;

  // make sure is at least size valid
  if(!raw || len < 2) return NULL;
  memcpy(&nlen,raw,2);
  hlen = platform_short(nlen);
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

uint8_t *lob_get(lob_t p, char *key)
{
  if(!p || !key) return NULL;
  return NULL;
//  return j0g_str(key, lob_j0g(p), p->index);
}

void lob_set_base32(lob_t p, char *key, uint8_t *val, uint16_t vlen)
{
  return;
}

/*
lob_t lob_parse(unsigned char *raw, unsigned short len)
{
  lob_t p;
  uint16_t nlen, jlen;

  // make sure is at least size valid
  if(!raw || len < 2) return NULL;
  memcpy(&nlen,raw,2);
  jlen = platform_short(nlen);
  if(jlen > len-2) return NULL;

  // copy in and update pointers
  p = lob_new();
  if(!(p->raw = realloc(p->raw,len))) return lob_free(p);
  memcpy(p->raw,raw,len);
  p->json_len = jlen;
  p->json = p->raw+2;
  p->body_len = len-(2+p->json_len);
  p->body = p->raw+(2+p->json_len);
  
  // parse json (if any) and validate
  if(jlen >= 2 && js0n(p->json,p->json_len,p->js,JSONDENSITY)) return lob_free(p);
  
  return p;
}


int lob_json(lob_t p, unsigned char *json, unsigned short len)
{
  uint16_t nlen;
  void *ptr;
  if(!p) return 1;
  if(len >= 2 && js0n(json,len,p->js,JSONDENSITY)) return 1;
  // new space and update pointers
  if(!(ptr = realloc(p->raw,2+len+p->body_len))) return 1;
  p->raw = (unsigned char *)ptr;
  p->json = p->raw+2;
  p->body = p->raw+(2+len);
  // move the body forward to make space
  memmove(p->body,p->raw+(2+p->json_len),p->body_len);
  // copy in new json
  memcpy(p->json,json,len);
  p->json_len = len;
  nlen = platform_short(len);
  memcpy(p->raw,&nlen,2);
  free(p->jsoncp);
  p->jsoncp = NULL;
  return 0;
}

unsigned char *lob_body(lob_t p, unsigned char *body, unsigned short len)
{
  void *ptr;
  if(!p) return NULL;
  if(!(ptr = realloc(p->raw,2+len+p->json_len))) return NULL;
  p->raw = (unsigned char *)ptr;
  p->json = p->raw+2;
  p->body = p->raw+(2+p->json_len);
  if(body) memcpy(p->body,body,len); // allows lob_body(p,NULL,100) to allocate space
  p->body_len = len;
  return p->body;
}

void lob_append(lob_t p, unsigned char *chunk, unsigned short len)
{
  void *ptr;
  if(!p || !chunk || !len) return;
  if(!(ptr = realloc(p->raw,2+len+p->body_len+p->json_len))) return;
  p->raw = (unsigned char *)ptr;
  p->json = p->raw+2;
  p->body = p->raw+(2+p->json_len);
  memcpy(p->body+p->body_len,chunk,len);
  p->body_len += len;
}

// TODO allow empty val to remove existing
void lob_set(lob_t p, char *key, char *val, int vlen)
{
  unsigned char *json, *at, *eval;
  int existing, klen, len, evlen;

  if(!p || !key || !val) return;
  if(p->json_len < 2) lob_json(p, (unsigned char*)"{}", 2);
  klen = strlen(key);
  if(!vlen) vlen = strlen(val); // convenience

  // make space and copy
  if(!(json = malloc(klen+vlen+p->json_len+4))) return;
  memcpy(json,p->json,p->json_len);

  // if it's already set, replace the value
  existing = j0g_val(key,(char*)p->json,p->js);
  if(existing)
  {
    // looks ugly, but is just adjusting the space avail for the value to the new size
    eval = json+p->js[existing];
    evlen = p->js[existing+1];
    // if existing was in quotes, include them
    if(*(eval-1) == '"')
    {
      eval--;
      evlen += 2;
    }
    memmove(eval+vlen,eval+evlen,(json+p->json_len) - (eval+evlen)); // frowney face
    memcpy(eval,val,vlen);
    len = p->json_len - evlen;
    len += vlen;
  }else{
    at = json+(p->json_len-1); // points to the "}"
    // if there's other keys already, add comma
    if(p->js[0])
    {
      *at = ','; at++;
    }
    *at = '"'; at++;
    memcpy(at,key,klen); at+=klen;
    *at = '"'; at++;
    *at = ':'; at++;
    memcpy(at,val,vlen); at+=vlen;
    *at = '}'; at++;
    len = at - json;
  }
  lob_json(p, json, len);
  free(json);
}

void lob_set_printf(lob_t p, char *key, const char *format, ...)
{
  va_list ap, cp;
  int len;
  char *val;

  if(!p || !key || !format) return;

  va_start(ap, format);
  va_copy(cp, ap);

  len = vsnprintf(NULL, 0, format, cp);
  if((val = malloc(len))) vsprintf(val, format, ap);
  va_end(ap);
  va_end(cp);

  lob_set_str(p, key, val);
}

void lob_set_int(lob_t p, char *key, int val)
{
  char num[32];
  if(!p || !key) return;
  sprintf(num,"%d",val);
  lob_set(p, key, num, 0);
}

void lob_set_str(lob_t p, char *key, char *val)
{
  char *escaped;
  int i, len, vlen = strlen(val);
  if(!p || !key || !val) return;
  if(!(escaped = malloc(vlen*2+2))) return; // enough space worst case
  len = 0;
  escaped[len++] = '"';
  for(i=0;i<vlen;i++)
  {
    if(val[i] == '"' || val[i] == '\\') escaped[len++]='\\';
    escaped[len++]=val[i];
  }
  escaped[len++] = '"';
  lob_set(p, key, escaped, len);
  free(escaped);
}

// internal to create/use a copy of the json
char *lob_j0g(lob_t p)
{
  if(!p) return NULL;
  if(p->jsoncp) return p->jsoncp;
  if(!(p->jsoncp = malloc(p->json_len+1))) return NULL;
  memcpy(p->jsoncp,p->json,p->json_len);
  p->jsoncp[p->json_len] = 0;
  return p->jsoncp;
}

char *lob_get_str(lob_t p, char *key)
{
  if(!p || !key) return NULL;
  return j0g_str(key, lob_j0g(p), p->js);
}

// returns ["0","1","2"] or {"0":"1","2":"3"}
char *lob_get_istr(lob_t p, int i)
{
  int j;
  if(!p) return NULL;
  for(j=0;p->js[j];j+=2)
  {
    if(i*2 != j) continue;
    return j0g_safe(j, lob_j0g(p), p->js);
  }
  return NULL;
}

// creates new packet from key:object
lob_t lob_get_packet(lob_t p, char *key)
{
  lob_t pp;
  int val;
  if(!p || !key) return NULL;

  val = j0g_val(key,(char*)p->json,p->js);
  if(!val) return NULL;

  pp = lob_new();
  lob_json(pp, p->json+p->js[val], p->js[val+1]);
  return pp;
}

// list of packet->next from key:[{},{}]
lob_t lob_get_packets(lob_t p, char *key)
{
  int i;
  lob_t parr, pent, plast, pret = NULL;
  if(!p || !key) return NULL;

  parr = lob_get_packet(p, key);
  if(!parr || *parr->json != '[')
  {
    lob_free(parr);
    return NULL;
  }

  // parse each object in the array, link together
	for(i=0;parr->js[i];i+=2)
	{
    pent = lob_new();
    lob_json(pent, parr->json+parr->js[i], parr->js[i+1]);
    if(!pret) pret = pent;
    else plast->next = pent;
    plast = pent;
	}

  lob_free(parr);
  return pret;
}

// count of keys
int lob_keys(lob_t p)
{
  int i;
  if(!p || !p->js[0]) return 0;
  for(i=0;p->js[i];i+=2);
  i = i/2; // i is start,len pairs
  if(i % 2) return 0; // must be even number for key:val pairs
  return i/2;
}

int pkeycmp(void *s, const void *a, const void *b)
{
  unsigned short *aa = (unsigned short *)a;
  unsigned short *bb = (unsigned short *)b;
  char *str = s;
  unsigned short len = aa[1];
  if(bb[1] < aa[1]) len = bb[1]; // take shortest
  return strncmp(str+aa[0],str+bb[0],len);
}

// alpha sort the keys
void lob_sort(lob_t p)
{
  int keys = lob_keys(p);
  if(!keys) return;
  util_sort(p->js,keys,sizeof(unsigned short)*4,pkeycmp,p->json);
}

int lob_cmp(lob_t a, lob_t b)
{
  int i = 0;
  char *str;
  if(!a || !b) return -1;
  if(a->body_len != b->body_len) return -1;
  if(lob_keys(a) != lob_keys(b)) return -1;

  lob_sort(a);
  lob_sort(b);
  while((str = lob_get_istr(a,i)))
  {
    if(strcmp(str,lob_get_istr(b,i)) != 0) return -1;
    i++;
  }

  return memcmp(a->body,b->body,a->body_len);
}

void lob_set_json(lob_t p, lob_t json)
{
  char *part;
  int i = 0;

  while((part = lob_get_istr(json,i)))
  {
    lob_set_str(p,part,lob_get_istr(json,i+1));
    i += 2;
  }
}


// return the null-terminated string value matching the given key
char *j0g_str(const char *key, char *json, const unsigned short *index)
{
  int val = j0g_val(key, json, index);
  if(!val) return NULL;
  return j0g_safe(val, json, index);
}

// null terminate and unescape any string at this value
char *j0g_safe(int val, char *json, const unsigned short *index)
{
  char *str, *cursor;
  *(json+(index[val]+index[val+1])) = 0; // null terminate
  // unescape stuff
  for(cursor=str=json+index[val]; *cursor; cursor++,str++)
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
  return (char*)json+index[val];
}

// return 1 if the value is the bool value true, number 1, or even the string "true", false otherwise
int j0g_test(const char *key, char *json, const unsigned short *index)
{
  char *val = j0g_str(key, json, index);
  if(!val) return 0;
  if(strcmp(val, "true") == 0) return 1;
  if(strcmp(val, "1") == 0) return 1;
  return 0;
}

// return the index offset of the value matching the given key
int j0g_val(const char *key, char *json, const unsigned short *index)
{
  if(!key || !json) return 0;
  int i, klen = strlen(key);
	for(i=0;index[i];i+=4)
	{
    if(klen == index[i+1] && strncmp(key,(char*)json+index[i],klen) == 0) return i+2;
	}
  return 0;
}

*/