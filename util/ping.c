#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "switch.h"
#include "util.h"
#include "ext.h"
#include "util_unix.h"
#include "crypt.h"

int main(void)
{
  unsigned char buf[2048];
  switch_t s;
  bucket_t seeds;
  chan_t c, c2;
  packet_t p;
  path_t from;
  int sock, len, blen;
  struct	sockaddr_in sad, sa;

  crypt_init();
  s = switch_new(0);

  switch_init(s,util_file2packet("id.json"));
  if(!s->id)
  {
    printf("failed to load id.json: %s\n", crypt_err());
    return -1;
  }
  printf("loaded hashname %s\n",s->id->hexname);

  seeds = bucket_load(s->index, "seeds.json");
  if(!seeds || !bucket_get(seeds, 0))
  {
    printf("failed to load seeds.json: %s\n", crypt_err());
    return -1;
  }

  // create a udp socket
  if( (sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 )
  {
	  printf("failed to create socket\n");
	  return -1;
  }
  memset((char *)&sad,0,sizeof(sad));
  memset((char *)&sa,0,sizeof(sa));
  sa.sin_family = sad.sin_family = AF_INET;
  sad.sin_port = htons(0);
  sad.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (sock, (struct sockaddr *)&sad, sizeof(sad)) < 0)
  {
	  printf("bind failed");
	  return -1;
  }

  // create/send a ping packet  
  c = chan_new(s, bucket_get(seeds, 0), "seek", 0);
  p = chan_packet(c);
  packet_set_str(p,"seek",s->id->hexname);  
  chan_send(c, p);

  while((p = switch_sending(s)))
  {
    if(util_cmp(p->out->type,"ipv4")!=0)
    {
      packet_free(p);
      continue;
    }
    printf("sending %s packet %d %s\n",p->json_len?"open":"line",packet_len(p),path_json(p->out));
    path2sa(p->out, &sa);
    if(sendto(sock, packet_raw(p), packet_len(p), 0, (struct sockaddr *)&sa, sizeof(sa))==-1)
    {
  	  printf("sendto failed\n");
  	  return -1;
    }
    packet_free(p);
  }
  
  from = path_new("ipv4");
  len = sizeof(sa);
  if ((blen = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&sa, (socklen_t *)&len)) == -1)
  {
	  printf("recvfrom failed\n");
	  return -1;
  }
  sa2path(&sa, from); // inits ip/port from sa
  p = packet_parse(buf,blen);
  printf("received %s packet %d %s\n", p->json_len?"open":"line", blen, path_json(from));
  switch_receive(s,p,from);

  while((p = switch_sending(s)))
  {
    if(util_cmp(p->out->type,"ipv4")!=0)
    {
      packet_free(p);
      continue;
    }
    printf("Sending %s packet %d %s\n",p->json_len?"open":"line",packet_len(p),path_json(p->out));
    path2sa(p->out, &sa);
    if(sendto(sock, packet_raw(p), packet_len(p), 0, (struct sockaddr *)&sa, sizeof(sa))==-1)
    {
  	  printf("sendto failed\n");
  	  return -1;
    }
    packet_free(p);
  }

  from = path_new("ipv4");
  len = sizeof(sa);
  while((blen = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&sa, (socklen_t *)&len)) != -1)
  {
    sa2path(&sa, from); // inits ip/port from sa
    p = packet_parse(buf,blen);
    printf("Received %s packet %d %s\n", p->json_len?"open":"line", blen, path_json(from));
    switch_receive(s,p,from);
  
    while((c2 = switch_pop(s)))
    {
      if(c2 == c)
      {
        printf("got pong state %d from %s see %s\n",c->ended,c->to->hexname,packet_get_str(chan_pop(c),"see"));
        return 0;
      }
      while((p = chan_pop(c)))
      {
        printf("unhandled channel packet %.*s\n", p->json_len, p->json);      
        packet_free(p);
      }
    }
  }
  printf("recvfrom failed\n");
  return -1;



  return 0;
}
