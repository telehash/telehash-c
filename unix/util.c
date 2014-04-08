#include <errno.h>
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

void util_sendall(switch_t s, int sock)
{
  struct	sockaddr_in sa;
  packet_t p;

  while((p = switch_sending(s)))
  {
    if(util_cmp(p->out->type,"ipv4")!=0)
    {
      packet_free(p);
      continue;
    }
    DEBUG_PRINTF("<<< %s packet %d %s",p->json_len?"open":"line",packet_len(p),path_json(p->out));
    path2sa(p->out, &sa);
    if(sendto(sock, packet_raw(p), packet_len(p), 0, (struct sockaddr *)&sa, sizeof(sa))==-1) printf("sendto failed\n");
    packet_free(p);
  }  
}

int util_readone(switch_t s, int sock, path_t in)
{
  unsigned char buf[2048];
  struct	sockaddr_in sa;
  int len, salen;
  packet_t p;
  
  salen = sizeof(sa);
  memset(&sa,0,salen);
  len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&sa, (socklen_t *)&salen);

  if(len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
  if(len <= 0) return 0;

  sa2path(&sa,in); // inits ip/port from sa
  p = packet_parse(buf,len);
  DEBUG_PRINTF(">>> %s packet %d %s", p->json_len?"open":"line", len, path_json(in));
  switch_receive(s,p,in);
  return 0;
}

int util_server(int port, int ms)
{
  int sock;
  struct	sockaddr_in sad;
  struct timeval tv;

  // create a udp socket
  if( (sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 )
  {
    printf("failed to create socket\n");
    return -1;
  }
  memset(&sad,0,sizeof(sad));
  sad.sin_family = AF_INET;
  sad.sin_port = htons(port);
  sad.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (sock, (struct sockaddr *)&sad, sizeof(sad)) < 0)
  {
    perror("bind failed");
    return -1;
  }
  tv.tv_sec = 0;
  tv.tv_usec = ms*1000;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
  {
    perror("setsockopt");
    return -1;
  }
  return sock;
}

int util_loadjson(switch_t s)
{
  bucket_t seeds;
  hn_t hn;
  int i = 0;

  if(switch_init(s, util_file2packet("id.json"))) return -1;

  seeds = bucket_load(s->index, "seeds.json");
  if(!seeds) return -1;
  while((hn = bucket_get(seeds, i)))
  {
    switch_seed(s,hn);
    i++;
  }
  bucket_free(seeds);
  return 0;
}
