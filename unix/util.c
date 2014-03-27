#include <string.h>
#include "util_unix.h"

void path2sa(path_t p, struct sockaddr_in *sa)
{
  memset(sa,0,sizeof(struct sockaddr_in));
  sa->sin_family = AF_INET;
  if(util_cmp("ipv4",p->type)==0) inet_aton(p->ip, &(sa->sin_addr));
  sa->sin_port = htons(p->port);
}

void sa2path(struct sockaddr_in *sa, path_t p)
{
  strcpy(p->ip,inet_ntoa(sa->sin_addr));
  p->port = ntohs(sa->sin_port);
}

packet_t util_file2packet(char *file)
{
  unsigned char *buf;
  int len;
  struct stat fs;
  FILE *fd;
  packet_t p;
  
  fd = fopen(file,"rb");
  if(!fd) return NULL;
  if(fstat(fileno(fd),&fs) < 0) return NULL;
  
  buf = malloc(fs.st_size);
  len = fread(buf,1,fs.st_size,fd);
  fclose(fd);
  if(len != fs.st_size)
  {
    free(buf);
    return NULL;
  }
  
  p = packet_new();
  packet_json(p, buf, len);
  return p;
}

bucket_t bucket_load(xht_t index, char *file)
{
  packet_t p, p2;
  hn_t hn;
  bucket_t b = NULL;
  int i;

  p = util_file2packet(file);
  if(!p) return b;
  if(*p->json != '{')
  {
    packet_free(p);
    return b;
  }

  // check each value, since js0n is key,len,value,len,key,len,value,len for objects
	for(i=0;p->js[i];i+=4)
	{
    p2 = packet_new();
    packet_json(p2, p->json+p->js[i+2], p->js[i+3]);
    hn = hn_fromjson(index, p2);
    packet_free(p2);
    if(!hn) continue;
    if(!b) b = bucket_new();
    bucket_add(b, hn);
	}

  packet_free(p);
  return b;
}
