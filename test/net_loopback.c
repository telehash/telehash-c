#include "net_loopback.h"
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
  
  net_loopback_t pair = net_loopback_new(meshA,meshB);
  fail_unless(pair);
  
  link_t linkAB = link_get(meshA, meshB->id);
  link_t linkBA = link_get(meshB, meshA->id);
  fail_unless(linkAB);
  fail_unless(linkBA);

  fail_unless(link_resync(linkAB));
  fail_unless(link_up(linkAB));
  fail_unless(link_up(linkBA));
  
  fail_unless(mesh_process(meshA,1));
  fail_unless(mesh_linked(meshA, meshB->id));
  fail_unless(mesh_unlink(linkAB));
  fail_unless(mesh_process(meshA,1));
  fail_unless(!mesh_linked(meshA, meshB->id));

  return 0;
}

