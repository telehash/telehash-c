#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mesh.h"
#include "udp4.h"
#include "unit_test.h"

int main(int argc, char *argv[])
{
  lob_t id;
  mesh_t mesh;
  net_udp4_t udp4;
  char *paths;

  mesh = mesh_new(0);
  fail_unless(mesh_generate(mesh));
  mesh_on_discover(mesh,"auto",mesh_add); // auto-link anyone

  udp4 = net_udp4_new(mesh, NULL);

  id = lob_new();
  fail_unless(lob_set_raw(id,"keys",(char*)mesh->keys->head,mesh->keys->head_len));
  paths = malloc(udp4->path->head_len+2);
  sprintf(paths,"[%s]",lob_json(udp4->path));
  lob_set_raw(id,"paths",paths,udp4->path->head_len+2);
  printf("%s\n",lob_json(id));
  fflush(stdout);

  while(net_udp4_receive(udp4));

  return 0;
}
