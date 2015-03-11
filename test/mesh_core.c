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
  fail_unless(link->csid);
  fail_unless(link->x);
  
  lob_t open = lob_new();
  lob_set(open,"type","test");
  lob_set_int(open,"c",e3x_exchange_cid(link->x, NULL));
  e3x_channel_t chan = link_channel(link, open);
  fail_unless(chan);

  pipe_t pipe = pipe_new("test");
  fail_unless(pipe);
  pipe_free(pipe);

  mesh_on_path(mesh, "test", net_test);
  pipe = link_path(link,lob_set(lob_new(),"type","test"));
  fail_unless(pipe);
  fail_unless(util_cmp(pipe->type,"test") == 0);
  fail_unless(link->pipes);
  fail_unless(link_pipes(link,NULL) == pipe);
  fail_unless(link_pipes(link,pipe) == NULL);

  return 0;
}

