#include "net_serial.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

int ABx = -1, BAx = -1;

int readerA(void)
{
  int ret;
  if(BAx < 0) return BAx; // empty
  ret = BAx;
  BAx = -1;
  return ret;
}

int writerA(uint8_t *buf, size_t len)
{
  if(!len || ABx >= 0) return 0; // full
  ABx = *buf; // copy one byte
  return 1;
}

int readerB(void)
{
  int ret;
  if(ABx < 0) return ABx; // empty
  ret = ABx;
  ABx = -1;
  return ret;
}

int writerB(uint8_t *buf, size_t len)
{
  if(!len || BAx >= 0) return 0; // full
  BAx = *buf; // copy one byte
  return 1;
}

int main(int argc, char **argv)
{
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = mesh_generate(meshA);
  fail_unless(secretsA);

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);
  
  net_serial_t netA = net_serial_new(meshA, NULL);
  fail_unless(netA);
  pipe_t pAB = net_serial_add(netA, "s1", readerA, writerA, 64);
  fail_unless(pAB);

  net_serial_t netB = net_serial_new(meshB, NULL);
  fail_unless(netB);
  pipe_t pBA = net_serial_add(netB, "s1", readerB, writerB, 64);
  fail_unless(pBA);

  link_t linkAB = link_pipe(link_keys(meshA, meshB->keys), pAB);
  link_t linkBA = link_pipe(link_keys(meshB, meshA->keys), pBA);
  fail_unless(linkAB);
  fail_unless(linkBA);

  link_sync(linkAB);
  // let serial go go go
  int loop;
  for(loop = 1000; loop; loop--)
  {
    net_serial_loop(netB);
    net_serial_loop(netA);
  }
  fail_unless(e3x_exchange_out(linkBA->x,0) >= e3x_exchange_out(linkAB->x,0));
  fail_unless(e3x_exchange_out(linkBA->x,0) == e3x_exchange_out(linkAB->x,0));


  return 0;
}

