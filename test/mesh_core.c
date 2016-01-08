#include "mesh.h"
#include "unit_test.h"

void net_send(pipe_t pipe, lob_t packet, link_t link)
{
  
}

pipe_t net_test(link_t link, lob_t path)
{
  fail_unless(path);
  pipe_t pipe = pipe_new("test");
  pipe->path = lob_copy(path);
  pipe->send = &net_send;
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
  lob_free(secrets);
  
  lob_t idB = e3x_generate();
  hashname_t hnB = hashname_vkeys(lob_linked(idB));
  fail_unless(hnB);
  link_t link = link_get(mesh,hnB);
  fail_unless(link);
  fail_unless(strlen(hashname_char(link->id)) == 52);
  fail_unless(link->csid == 0x01);
  
  fail_unless(link_keys(mesh,lob_linked(idB)) == link);
  fail_unless(link->csid > 0x01);
  fail_unless(link->x);
  lob_free(idB);
  
  lob_t open = lob_new();
  lob_set(open,"type","test");
  lob_set_int(open,"c",e3x_exchange_cid(link->x, NULL));
  chan_t chan = link_chan(link, open);
  fail_unless(chan);
  lob_free(open);

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
  
  fail_unless(strlen(lob_json(mesh_json(mesh))) > 100);
  LOG("json %s",lob_json(lob_array(mesh_links(mesh))));
  fail_unless(strlen(lob_json(lob_array(mesh_links(mesh)))) > 10);

  fail_unless(mesh_process(mesh, 1));

  link_free(link);
  mesh_free(mesh);

  return 0;
}

