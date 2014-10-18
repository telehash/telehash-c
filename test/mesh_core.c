#include "mesh.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  mesh_t mesh = mesh_new(3);
  fail_unless(mesh);
  lob_t secrets = mesh_generate(mesh);
  fail_unless(secrets);
  fail_unless(mesh->self);
//  fail_unless(mesh->id);
  
  uint8_t dummy[32];
  link_t link = link_new(mesh,hashname_new(dummy));
  fail_unless(link);
  fail_unless(strlen(link->id->hashname) == 52);

  pipe_t pipe = pipe_new("test");
  fail_unless(pipe);

  return 0;
}

