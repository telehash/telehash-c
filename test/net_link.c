#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mesh.h"
#include "udp4.h"
#include "ext.h"
#include "unit_test.h"

lob_t status;

// exit as soon as the link is up
void link_check(link_t link)
{
  status = ext_link_status(link,NULL);
}

int main(int argc, char *argv[])
{
  lob_t id;
  mesh_t mesh;
  net_udp4_t udp4;
  char *paths;

  mesh = mesh_new(0);
  fail_unless(mesh_generate(mesh));
  mesh_on_discover(mesh,"auto",mesh_add); // accept anyone
  ext_link_auto(mesh); // allow all link channels
  mesh_on_link(mesh, "test", link_check);
  status = NULL;

  udp4 = net_udp4_new(mesh, NULL);

  id = lob_new();
  fail_unless(lob_set_raw(id,"keys",(char*)mesh->keys->head,mesh->keys->head_len));
  paths = malloc(udp4->path->head_len+2);
  sprintf(paths,"[%s]",lob_json(udp4->path));
  lob_set_raw(id,"paths",paths,udp4->path->head_len+2);
  printf("%s\n",lob_json(id));
  fflush(stdout);

  while(net_udp4_receive(udp4) && !status);

  return 0;
}
