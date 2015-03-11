#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mesh.h"
#include "net_udp4.h"
#include "ext.h"

static uint8_t status = 0;

void pong(link_t link, lob_t ping, void *arg)
{
  LOG("pong'd %s",lob_json(ping));
  status = 1;
}

// ping as soon as the link is up
void link_check(link_t link)
{
  if(link_up(link))
  {
    LOG("link is up, pinging");
    path_ping(link,pong,NULL);
  }
}

int main(int argc, char *argv[])
{
  lob_t id;
  mesh_t mesh;
  net_udp4_t udp4;
  char *paths;

  mesh = mesh_new(0);
  mesh_generate(mesh);
  mesh_on_discover(mesh,"auto",mesh_add); // accept anyone
  mesh_on_link(mesh, "test", link_check); // testing the event being triggered
  status = 0;

  udp4 = net_udp4_new(mesh, NULL);

  id = lob_new();
  lob_set_raw(id,"keys",0,(char*)mesh->keys->head,mesh->keys->head_len);
  paths = malloc(udp4->path->head_len+2);
  sprintf(paths,"[%s]",lob_json(udp4->path));
  lob_set_raw(id,"paths",0,paths,udp4->path->head_len+2);
  printf("%s\n",lob_json(id));
  fflush(stdout);

  while(net_udp4_receive(udp4) && !status);

  return 0;
}
