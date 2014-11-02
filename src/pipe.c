#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"
#include "platform.h"
#include "pipe.h"

pipe_t pipe_new(char *type)
{
  pipe_t p;
  if(!type) return LOG("no type");

  if(!(p = malloc(sizeof (struct pipe_struct)))) return NULL;
  memset(p,0,sizeof (struct pipe_struct));
  p->type = strdup(type);
  return p;
}

pipe_t pipe_free(pipe_t p)
{
  free(p->type);
  if(p->id) free(p->id);
  if(p->path) lob_free(p->path);
  if(p->notify) LOG("pipe free'd leaking notifications");
  free(p);
  return NULL;
}

/*

* pipes are created by transports (often when given a path)
* should always return the same pipe for the same path
* links keep a list of 'seens', track state of each pipe
* pipes have a list of lobs, one for every active listener
* pipes send the lobs for keepalive, set err for close, must be processed then free'd

pipe_t pipe_new(char *type)
{
  if(!strstr("ipv4 ipv6 relay local http", type)) return NULL;
  pipe_t p;
  if(!(p = malloc(sizeof (struct pipe_struct)))) return NULL;
  memset(p,0,sizeof (struct pipe_struct));
  memcpy(p->type,type,strlen(type)+1);
  return p;
}

void pipe_free(pipe_t p)
{
  if(p->id) free(p->id);
  if(p->json) free(p->json);
  free(p);
}

pipe_t pipe_parse(char *json, int len)
{
  unsigned short js[64];
  pipe_t p;
  
  if(!json) return NULL;
  if(!len) len = strlen(json);
  js0n((unsigned char*)json, len, js, 64);
  if(!j0g_val("type",json,js)) return NULL;
  p = pipe_new(j0g_str("type",json,js));

  // just try to set all possible attributes
  pipe_ip(p, j0g_str("ip",json,js));
  if(j0g_str("port",json,js)) pipe_port(p, (uint16_t)strtol(j0g_str("port",json,js),NULL,10));
  pipe_id(p, j0g_str("id",json,js));
  pipe_http(p, j0g_str("http",json,js));
  
  return p;
}

char *pipe_id(pipe_t p, char *id)
{
  if(!id) return p->id;
  if(p->id) free(p->id);
  if(!(p->id = malloc(strlen(id)+1))) return NULL;
  memcpy(p->id,id,strlen(id)+1);
  return p->id;
}

// overloads our .id for the http value
char *pipe_http(pipe_t p, char *http)
{
  return pipe_id(p,http);
}

char *pipe_ip(pipe_t p, char *ip)
{
  if(!ip) return p->ip;
  if(strlen(ip) > 45) return NULL;
  strcpy(p->ip,ip);
  return p->ip;
}

char *pipe_ip4(pipe_t p, uint32_t ip)
{
  uint8_t *ip4 = (uint8_t*)&ip;
  if(!ip) return p->ip;
  sprintf(p->ip,"%d.%d.%d.%d",ip4[0],ip4[1],ip4[2],ip4[3]);
  return p->ip;
}

uint16_t pipe_port(pipe_t p, uint16_t port)
{
  if(port) p->port = port;
  return p->port;
}


char *pipe_json(pipe_t p)
{
  int len;
  char *json;
  
  if(!p) return NULL;
  if(p->json) free(p->json);
  len = 12+strlen(p->type);
  if(p->ip) len += strlen(p->ip)+8+13;
  if(p->id) len += strlen(p->id)+8;
  len = len * 2; // double for any worst-case escaping
  if(!(p->json = malloc(len))) return NULL;
  json = p->json;
  memset(json,0,len);
  sprintf(json, "{\"type\":%c%s%c",'"',p->type,'"');
  if(strstr("ipv4 ipv6", p->type))
  {
    if(*p->ip) sprintf(json+strlen(json), ",\"ip\":%c%s%c",'"',p->ip,'"');
    if(p->port) sprintf(json+strlen(json), ",\"port\":%hu", p->port);
  }
  if(p->id)
  {
    if(util_cmp("http",p->type)==0) strcpy(json+strlen(json), ",\"http\":\"");
    else strcpy(json+strlen(json), ",\"id\":\"");
    len = 0;
    while(p->id[len])
    {
      if(p->id[len] == '"') json[strlen(json)] = '\\';
      json[strlen(json)] = p->id[len];
      len++;
    }
    strcpy(json+strlen(json), "\"");
  }
  json[strlen(json)] = '}';
  return p->json;
}

int pipe_match(pipe_t p1, pipe_t p2)
{
  if(!p1 || !p2) return 0;
  if(p1 == p2) return 1;
  if(util_cmp(p1->type,p2->type) != 0) return 0;
  if(strstr(p1->type,"ipv"))
  {
    if(util_cmp(p1->ip, p2->ip) == 0 && p1->port == p2->port) return 1;
  }else{
    if(util_cmp(p1->id, p2->id) == 0) return 1;
  }
  return 0;
}

int pipe_alive(pipe_t p)
{
  unsigned long now;
  if(!p) return 0;
  now = platform_seconds();
  if((now - p->tin) < 30) return 1;
  return 0;
}

// tell if pipe is local or public, 1 = local 0 = public
int pipe_local(pipe_t p)
{
  if(!p) return -1;
  // TODO, determine public types/IP ranges
  return 1;
}

pipe_t pipe_copy(pipe_t p)
{
  pipe_t p2 = pipe_parse(pipe_json(p),0);
  p2->arg = p->arg;
  return p2;
}
*/