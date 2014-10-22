#include "mesh.h"
#include "unit_test.h"

pipe_t net_test(link_t link, lob_t path)
{
  fail_unless(path);
  pipe_t pipe = pipe_new("test");
  pipe->path = lob_copy(path);
  return pipe;
}

int main(int argc, char **argv)
{
  mesh_t mesh = mesh_new(3);
  fail_unless(mesh);
  lob_t secrets = mesh_generate(mesh);
  fail_unless(secrets);
  fail_unless(mesh->self);
  fail_unless(mesh->id);
  
  lob_t idB = e3x_generate();
  hashname_t hnB = hashname_keys(lob_linked(idB));
  fail_unless(hnB);
  link_t link = link_get(mesh,hnB->hashname);
  fail_unless(link);
  fail_unless(strlen(link->id->hashname) == 52);
  fail_unless(link->csid == 0);
  
  fail_unless(link_keys(mesh,lob_linked(idB)) == link);
  fail_unless(link->csid == 0x1a);
  fail_unless(link->x);

  pipe_t pipe = pipe_new("test");
  fail_unless(pipe);
  pipe_free(pipe);

  fail_unless(mesh_net(mesh, net_test) == 0);
  pipe = link_path(link,lob_set(lob_new(),"type","test"));
  fail_unless(pipe);
  fail_unless(util_cmp(pipe->type,"test") == 0);
  fail_unless(link->pipes);


  return 0;
}

