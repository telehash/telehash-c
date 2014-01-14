#include "path.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "js0n.h"
#include "j0g.h"


path_t path_new(char *type)
{
  if(!strstr("ipv4 ipv6 relay bridge http", type)) return NULL;
  path_t p = malloc(sizeof (struct path_struct));
  bzero(p,sizeof (struct path_struct));
  memcpy(p->type,type,strlen(type)+1);
  p->sa.sin_family = AF_INET;
  return p;
}

void path_free(path_t p)
{
  if(p->id) free(p->id);
  if(p->json) free(p->json);
  free(p);
}

path_t path_parse(unsigned char *json, int len)
{
  unsigned short js[64];
  path_t p;
  
  if(!json || !len) return NULL;
  js0n(json, len, js, 64);
  if(!j0g_val("type",(char*)json,js)) return NULL;
  p = path_new(j0g_str("type",(char*)json,js));

  // just try to set all possible attributes
  path_ip(p, j0g_str("ip",(char*)json,js));
  if(j0g_val("port",(char*)json,js)) path_port(p, atoi(j0g_str("port",(char*)json,js)));
  path_id(p, j0g_str("id",(char*)json,js));
  path_http(p, j0g_str("http",(char*)json,js));
  
  return p;
}

char *path_id(path_t p, char *id)
{
  if(!id) return p->id;
  if(p->id) free(p->id);
  p->id = strdup(id);
  return p->id;
}

// overloads our .id for the http value
char *path_http(path_t p, char *http)
{
  if(!http) return p->id;
  if(p->id) free(p->id);
  p->id = strdup(http);
  return p->id;
}

char *path_ip(path_t p, char *ip)
{
  if(!ip) return p->ip;
  if(strlen(ip) > 45) return NULL;
  strcpy(p->ip,ip);
  if(strcmp("ipv4",p->type)==0) inet_aton(ip, &(p->sa.sin_addr));
  return p->ip;
}

int path_port(path_t p, int port)
{
  if(port > 0 && port <= 65535) p->port = port;
  p->sa.sin_port = htons(p->port);
  return p->port;
}

unsigned char *path_json(path_t p)
{
  int len;
  char *json;
  
  if(!p) return NULL;
  if(p->json) free(p->json);
  len = 32+strlen(p->type)+strlen(p->ip);
  if(p->id) len += strlen(p->id)*2; // double for possible escaping
  p->json = malloc(len);
  json = (char*)p->json;
  bzero(json,len);
  sprintf(json, "{\"type\":\"%s\"",p->type);
  if(strstr("ipv4 ipv6", p->type))
  {
    if(*p->ip) sprintf(json+strlen(json), ",\"ip\":\"%s\"", p->ip);
    if(p->port) sprintf(json+strlen(json), ",\"port\":%d", p->port);
  }
  if(p->id)
  {
    if(strcmp("http",p->type)==0) strcpy(json+strlen(json), ",\"http\":\"");
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

int path_match(path_t p1, path_t p2)
{
  if(!p1 || !p2) return 0;
  if(p1 == p2) return 1;
  if(strcmp(p1->type,p2->type) != 0) return 0;
  if(strstr(p1->type,"ipv"))
  {
    if(strcmp(p1->ip, p2->ip) == 0 && p1->port == p2->port) return 1;
  }else{
    if(strcmp(p1->id, p2->id) == 0) return 1;
  }
  return 0;
}

int path_alive(path_t p)
{
  if(!p) return 0;
  return 0;
}
