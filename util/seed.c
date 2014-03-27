#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "switch.h"
#include "util.h"
#include "ext.h"
#include "util_unix.h"

void sendall(switch_t s, int sock)
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
    printf("<<< %s packet %d %s\n",p->json_len?"open":"line",packet_len(p),path_json(p->out));
    path2sa(p->out, &sa);
    if(sendto(sock, packet_raw(p), packet_len(p), 0, (struct sockaddr *)&sa, sizeof(sa))==-1) printf("sendto failed\n");
    packet_free(p);
  }  
}

int readone(switch_t s, int sock, path_t in)
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
  printf(">>> %s packet %d %s\n", p->json_len?"open":"line", len, path_json(in));
  switch_receive(s,p,in);
  return 0;
}

int server(int port)
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
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
  {
    perror("setsockopt");
    return -1;
  }
  return sock;
}

int main(void)
{
  switch_t s;
  bucket_t seeds;
  chan_t c;
  packet_t p;
  path_t in;
  int sock;

  crypt_init();
  s = switch_new();

  if(switch_init(s, util_file2packet("id.json")))
  {
    printf("failed to init switch: %s\n", crypt_err());
    return -1;
  }
  printf("loaded hashname %s\n",s->id->hexname);

  seeds = bucket_load(s->index, "seeds.json");
  if(!seeds || !bucket_get(seeds, 0))
  {
    printf("failed to load seeds.json: %s\n", crypt_err());
    return -1;
  }

  if((sock = server(0)) <= 0) return -1;

  // create/send a ping packet  
  c = chan_new(s, bucket_get(seeds, 0), "link", 0);
  p = chan_packet(c);
  switch_send(s, p);
  sendall(s,sock);

  in = path_new("ipv4");
  while(readone(s, sock, in) == 0)
  {
    switch_loop(s);

    while((c = switch_pop(s)))
    {
      printf("channel active %d %s %s\n",c->state,c->hexid,c->to->hexname);
      if(util_cmp(c->type,"connect") == 0) ext_connect(c);
      if(util_cmp(c->type,"thtp") == 0) ext_thtp(c);
      if(util_cmp(c->type,"link") == 0) ext_link(c);
      if(util_cmp(c->type,"seek") == 0) ext_seek(c);
      if(util_cmp(c->type,"path") == 0) ext_path(c);
      while((p = chan_pop(c)))
      {
        printf("unhandled channel packet %.*s\n", p->json_len, p->json);      
        packet_free(p);
      }
    }

    sendall(s,sock);
  }

  perror("exiting");
  return 0;
}
