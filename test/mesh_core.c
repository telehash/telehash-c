#include "mesh.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  mesh_t mesh = mesh_new(3);
  fail_unless(mesh);
  
  return 0;
}

