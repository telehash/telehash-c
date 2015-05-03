#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "mesh.h"
#include "net_serial.h"
#include "ext.h"
#include "unit_test.h"

int reader(void)
{
  uint8_t c1;
  if(read(0, &c1, 1) <= 0) return -1;
  return c1;
}

int writer(uint8_t *buf, size_t len)
{
  return write(1, buf, len);
}

static uint8_t status = 0;

// exit as soon as the link is up
void link_check(link_t link)
{
  LOG("CHECK %s",link->id->hashname);
  status = link_up(link) ? 1 : 0;
}

int main(int argc, char *argv[])
{
  mesh_t mesh;
  net_serial_t s;

  mesh = mesh_new(0);
  fail_unless(mesh_generate(mesh));
  mesh_on_discover(mesh,"auto",mesh_add); // accept anyone
  mesh_on_link(mesh, "test", link_check); // testing the event being triggered
  status = 0;

  s = net_serial_new(mesh, NULL);
  net_serial_add(s, "stdout", reader, writer, 64);

  fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK); // nonblocking reads
  setvbuf(stdout, NULL, _IONBF, 0); // no buffering
  net_serial_send(s, "stdout", mesh_json(mesh));

  LOG("entering loop");

  while(net_serial_loop(s) && !status);
  
  LOG("success!");

  return 0;
}
