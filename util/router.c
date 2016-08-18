#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mesh.h"
#include "util_unix.h"
#include "net_udp4.h"
#include "ext.h"

int main(int argc, char *argv[])
{
  lob_t id, options, json;
  mesh_t mesh;
  net_udp4_t udp4;
  int port = 0;
  int link = 0;

  // support "router 12345 54321" first arg listen, second to establish link to
  if(argc >= 3)
  {
    port = atoi(argv[1]);
    link = atoi(argv[2]);
  }

  id = util_fjson("id.json");
  if(!id) return -1;
  
  mesh = mesh_new();
  mesh_load(mesh,lob_get_json(id,"secrets"),lob_get_json(id,"keys"));
  mesh_on_discover(mesh,"auto",mesh_add); // auto-link anyone

  options = lob_new();
  lob_set_int(options,"port",port);

  udp4 = net_udp4_new(mesh, options);
  util_sock_timeout(net_udp4_socket(udp4),100);

  json = mesh_json(mesh);
  printf("%s\n",lob_json(json));
  printf("using port %u\n",net_udp4_port(udp4));

  if(link)
  {
    net_udp4_direct(udp4, json, "127.0.0.1", link);
    printf("sent hello to %d\n",link);
  }

  while(net_udp4_process(udp4));

  perror("exiting");
  return 0;
}
