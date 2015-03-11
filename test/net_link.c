#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mesh.h"
#include "net_udp4.h"
#include "ext.h"
#include "unit_test.h"

static uint8_t status = 0;

// exit as soon as the link is up
void link_check(link_t link)
{
  status = link_up(link) ? 1 : 0;
}

int main(int argc, char *argv[])
{
  lob_t id;
  mesh_t mesh;
  net_udp4_t udp4;

  mesh = mesh_new(0);
  fail_unless(mesh_generate(mesh));
  mesh_on_discover(mesh,"auto",mesh_add); // accept anyone
  mesh_on_link(mesh, "test", link_check); // testing the event being triggered
  status = 0;

  udp4 = net_udp4_new(mesh, NULL);

  id = mesh_json(mesh);
  printf("%s\n",lob_json(id));
  fflush(stdout);

  while(net_udp4_receive(udp4) && !status);

  return 0;
}
