#include "net_serial.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

// hard coding two fake serial buffers to each other
uint8_t ABx[64], BAx[64];
int ABy = -1, BAy = -1;

int readerA(void)
{
  int ret;
  if(BAy <= 0) return -1; // empty
  ret = BAx[0];
  BAy--;
  if(BAy > 0) memmove(BAx,BAx+1,BAy);
  return ret;
}

int writerA(uint8_t *buf, size_t len)
{
  if(!len || ABy > 0 || len > 64) return 0; // full
  memcpy(ABx,buf,len);
  ABy = len;
  return len;
}

int readerB(void)
{
  int ret;
  if(ABy <= 0) return -1; // empty
  ret = ABx[0];
  ABy--;
  if(ABy > 0) memmove(ABx,ABx+1,ABy);
  return ret;
}

int writerB(uint8_t *buf, size_t len)
{
  if(!len || BAy > 0 || len > 64) return 0; // full
  memcpy(BAx,buf,len);
  BAy = len;
  return len;
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
  pipe_t pAB = net_serial_add(netA, "sAB", readerA, writerA, 64);
  fail_unless(pAB);

  net_serial_t netB = net_serial_new(meshB, NULL);
  fail_unless(netB);
  pipe_t pBA = net_serial_add(netB, "sBA", readerB, writerB, 64);
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

