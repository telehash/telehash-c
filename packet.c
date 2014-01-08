#include "packet.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "js0n.h"

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
  packet_t np = packet_new();
  packet_init(np, packet_raw(p), packet_len(p));
  return np;
}

void packet_free(packet_t p)
{
  free(p->raw);
  free(p);
}

void packet_init(packet_t p, unsigned char *raw, unsigned short len)
{
  uint16_t nlen = 2;
  p->raw = realloc(p->raw,len);
  memcpy(p->raw,raw,len);
  memcpy(&nlen,raw,2);
  p->json_len = ntohs(nlen);
  p->json = p->raw+2;
  p->body_len = len-(2+p->json_len);
  p->body = p->raw+(2+p->json_len);
  js0n(p->json,p->json_len,p->js,JSONDENSITY);
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
  p->raw = realloc(p->raw,2+len+p->body_len);
  // move the body forward to make space
  p->body = p->raw+(2+len);
  memmove(p->body,p->raw+(2+p->json_len),p->body_len);
  // copy in new json
  memcpy(p->raw+2,json,len);
  p->json_len = len;
  p->json = p->raw+2;
  nlen = htons(len);
  memcpy(p->raw,&nlen,2);
  js0n(p->json,p->json_len,p->js,JSONDENSITY);
}

void packet_body(packet_t p, unsigned char *body, unsigned short len)
{
  p->raw = realloc(p->raw,2+len+p->json_len);
  p->body = p->raw+(2+p->json_len);
  memcpy(p->body,body,len);
  p->body_len = len;
}
