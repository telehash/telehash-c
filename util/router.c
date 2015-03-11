#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mesh.h"
#include "util_unix.h"
#include "net_udp4.h"
#include "net_tcp4.h"
#include "ext.h"

int main(int argc, char *argv[])
{
  lob_t id, options, json;
  mesh_t mesh;
  net_udp4_t udp4;
  net_tcp4_t tcp4;
  int port = 0;

  if(argc==2)
  {
    port = atoi(argv[1]);
  }

  id = util_fjson("id.json");
  if(!id) return -1;
  
  mesh = mesh_new(0);
  mesh_load(mesh,lob_get_json(id,"secrets"),lob_get_json(id,"keys"));
  mesh_on_discover(mesh,"auto",mesh_add); // auto-link anyone
  mesh_on_open(mesh,"path",path_on_open); // add path support

  options = lob_new();
  lob_set_int(options,"port",port);

  udp4 = net_udp4_new(mesh, options);
  util_sock_timeout(udp4->server,100);

  tcp4 = net_tcp4_new(mesh, options);

  json = mesh_json(mesh);
  printf("%s\n",lob_json(json));
  printf("%s\n",mesh_uri(mesh, NULL));

  while(net_udp4_receive(udp4) && net_tcp4_loop(tcp4));

  /*
  if(util_loadjson(s) != 0 || (sock = util_server(0,1000)) <= 0)
  {
    printf("failed to startup %s or %s\n", strerror(errno), crypt_err());
    return -1;
  }

  printf("loaded hashname %s\n",s->id->hexname);

  // create/send a ping packet  
  c = chan_new(s, bucket_get(s->seeds, 0), "link", 0);
  p = chan_packet(c);
  chan_send(c, p);
  util_sendall(s,sock);

  in = path_new("ipv4");
  while(util_readone(s, sock, in) == 0)
  {
    switch_loop(s);

    while((c = switch_pop(s)))
    {
      printf("channel active %d %s %s\n",c->ended,c->hexid,c->to->hexname);
      if(util_cmp(c->type,"connect") == 0) ext_connect(c);
      if(util_cmp(c->type,"link") == 0) ext_link(c);
      if(util_cmp(c->type,"path") == 0) ext_path(c);
      while((p = chan_pop(c)))
      {
        printf("unhandled channel packet %.*s\n", p->json_len, p->json);      
        lob_free(p);
      }
    }

    util_sendall(s,sock);
  }
  */
  perror("exiting");
  return 0;
}
