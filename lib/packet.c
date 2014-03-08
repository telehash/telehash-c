#include "packet.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "js0n.h"
#include "j0g.h"
#include "hn.h"
#include "platform.h"

packet_t packet_new()
{
  packet_t p = malloc(sizeof (struct packet_struct));
  memset(p,0,sizeof (struct packet_struct));
  p->raw = malloc(2);
  memset(p->raw,0,2);
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
  if(p->jsoncp) free(p->jsoncp);
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
  jlen = platform_short(nlen);
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
  if(jlen >= 2 && js0n(p->json,p->json_len,p->js,JSONDENSITY)) return packet_free(p);
  
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

int packet_json(packet_t p, unsigned char *json, unsigned short len)
{
  uint16_t nlen;
  if(len >= 2 && js0n(json,len,p->js,JSONDENSITY)) return 1;
  // new space and update pointers
  p->raw = realloc(p->raw,2+len+p->body_len);
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

void packet_body(packet_t p, unsigned char *body, unsigned short len)
{
  p->raw = realloc(p->raw,2+len+p->json_len);
  p->json = p->raw+2;
  p->body = p->raw+(2+p->json_len);
  if(body) memcpy(p->body,body,len); // allows packet_body(p,NULL,100) to allocate space
  p->body_len = len;
}

void packet_set(packet_t p, char *key, char *val, int vlen)
{
  unsigned char *json, *at, *eval;
  int existing, klen, len, evlen;

  if(!p || !key || !val) return;
  if(p->json_len < 2) packet_json(p, (unsigned char*)"{}", 2);
  klen = strlen(key);
  if(!vlen) vlen = strlen(val); // convenience

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
  packet_set(p, key, num, 0);
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
  packet_set(p, key, escaped, len);
  free(escaped);
}

// internal to create/use a copy of the json
char *packet_j0g(packet_t p)
{
  if(!p) return NULL;
  if(p->jsoncp) return p->jsoncp;
  p->jsoncp = malloc(p->json_len+1);
  memcpy(p->jsoncp,p->json,p->json_len);
  p->jsoncp[p->json_len] = 0;
  return p->jsoncp;
}

char *packet_get_str(packet_t p, char *key)
{
  if(!p || !key) return NULL;
  return j0g_str(key, packet_j0g(p), p->js);
}

// returns ["0","1","2"]
char *packet_get_istr(packet_t p, int i)
{
  int j;
  if(!p) return NULL;
  for(j=0;p->js[j];j+=2)
  {
    if(i*2 != j) continue;
    return j0g_safe(j, packet_j0g(p), p->js);
  }
  return NULL;
}

// creates new packet from key:object
packet_t packet_get_packet(packet_t p, char *key)
{
  packet_t pp;
  int val;
  if(!p || !key) return NULL;

  val = j0g_val(key,(char*)p->json,p->js);
  if(!val) return NULL;

  pp = packet_new();
  packet_json(pp, p->json+p->js[val], p->js[val+1]);
  return pp;
}

// list of packet->next from key:[{},{}]
packet_t packet_get_packets(packet_t p, char *key)
{
  int i;
  packet_t parr, pent, plast, pret = NULL;
  if(!p || !key) return NULL;

  parr = packet_get_packet(p, key);
  if(!parr || *parr->json != '[')
  {
    packet_free(parr);
    return NULL;
  }

  // parse each object in the array, link together
	for(i=0;parr->js[i];i+=2)
	{
    pent = packet_new();
    packet_json(pent, parr->json+parr->js[i], parr->js[i+1]);
    if(!pret) pret = pent;
    else plast->next = pent;
    plast = pent;
	}

  packet_free(parr);
  return pret;
}
