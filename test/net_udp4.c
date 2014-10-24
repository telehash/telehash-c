#include "udp4.h"
#include "unit_test.h"

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
  
  net_udp4_t netA = net_udp4_new(meshA, 0);
  fail_unless(netA);
  
  return 0;
}

