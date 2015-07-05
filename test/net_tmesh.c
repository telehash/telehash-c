#include "net_tmesh.h"
#include "util_sys.h"
#include "util_unix.h"
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
  
  net_tmesh_t netA = net_tmesh_new(meshA, NULL, NULL);
  fail_unless(netA);
  fail_unless(netA->path);
  LOG("netA %.*s",netA->path->head_len,netA->path->head);


  return 0;
}

