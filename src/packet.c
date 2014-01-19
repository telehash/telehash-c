#include "packet.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "js0n.h"
#include "j0g.h"
#include "hn.h"

packet_t packet_new()
{
  packet_t p = malloc(sizeof (struct packet_struct));
  bzero(p,sizeof (struct packet_struct));
  p->raw = malloc(2);
  bzero(p->raw,2);
  return p;
}

packet_t packet_copy(packet_t p)
{
  packet_t np;
  np = packet_parse(packet_raw(p), packet_len(p));
  np->to = p->to;
  np->from = p->from;
  np->out = p->out;
  return np;
}

packet_t packet_chain(packet_t p)
{
  packet_t np = packet_new();
  np->chain = p;
  // copy in meta-pointers for convenience
  np->to = p->to;
  np->from = p->from;
  np->out = p->out;
  return np;
}

packet_t packet_free(packet_t p)
{
  if(p->chain) packet_free(p->chain);
  free(p->raw);
  free(p);
  return NULL;
}

packet_t packet_parse(unsigned char *raw, unsigned short len)
{
  packet_t p;
  uint16_t nlen, jlen;

  // make sure is at least size valid
  if(!raw || len < 2) return NULL;
  memcpy(&nlen,raw,2);
  jlen = ntohs(nlen);
  if(jlen > len-2) return NULL;

  // copy in and update pointers
  p = packet_new();
  p->raw = realloc(p->raw,len);
  memcpy(p->raw,raw,len);
  p->json_len = jlen;
  p->json = p->raw+2;
  p->body_len = len-(2+p->json_len);
  p->body = p->raw+(2+p->json_len);
  
  // parse json (if any) and validate
  if(jlen && js0n(p->json,p->json_len,p->js,JSONDENSITY)) return packet_free(p);
  
  return p;
}

unsigned char *packet_raw(packet_t p)
{
  return p->raw;
}

unsigned short packet_len(packet_t p)
{
  return 2+p->json_len+p->body_len;
}

void packet_json(packet_t p, unsigned char *json, unsigned short len)
{
  uint16_t nlen;
  // new space and update pointers
  p->raw = realloc(p->raw,2+len+p->body_len);
  p->json = p->raw+2;
  p->body = p->raw+(2+len);
  // move the body forward to make space
  memmove(p->body,p->raw+(2+p->json_len),p->body_len);
  // copy in new json
  memcpy(p->json,json,len);
  p->json_len = len;
  nlen = htons(len);
  memcpy(p->raw,&nlen,2);
  js0n(p->json,p->json_len,p->js,JSONDENSITY);
}

void packet_body(packet_t p, unsigned char *body, unsigned short len)
{
  p->raw = realloc(p->raw,2+len+p->json_len);
  p->json = p->raw+2;
  p->body = p->raw+(2+p->json_len);
  memcpy(p->body,body,len);
  p->body_len = len;
}

void packet_set(packet_t p, char *key, char *val)
{
  unsigned char *json, *at, *eval;
  int existing, vlen, klen, len, evlen;

  if(!p || !key || !val) return;
  if(!p->json_len) packet_json(p, (unsigned char*)"{}", 2);
  klen = strlen(key);
  vlen = strlen(val);

  // make space and copy
  json = malloc(klen+vlen+p->json_len+4);
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
    memmove(eval+vlen,eval+evlen,vlen);
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
  packet_json(p, json, len);
  free(json);
}

void packet_set_int(packet_t p, char *key, int val)
{
  char num[32];
  if(!p || !key) return;
  sprintf(num,"%d",val);
  packet_set(p, key, num);
}

void packet_set_str(packet_t p, char *key, char *val)
{
  char *escaped;
  int i, len, vlen = strlen(val);
  if(!p || !key || !val) return;
  escaped = malloc(vlen*2+2); // enough space worst case
  len = 0;
  escaped[len++] = '"';
  for(i=0;i<vlen;i++)
  {
    if(val[i] == '"' || val[i] == '\\') escaped[len++]='\\';
    escaped[len++]=val[i];
  }
  escaped[len++] = '"';
  escaped[len] = 0;
  packet_set(p, key, escaped);
  free(escaped);
}

char *packet_get_str(packet_t p, char *key)
{
  if(!p || !key) return NULL;
  return j0g_str(key, (char*)p->json, p->js);
}
