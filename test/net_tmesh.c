#include "net_tmesh.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

epoch_t test_init(tmesh_t net, epoch_t e)
{
  return epoch_busy(e, 5000); // 5ms
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
  
  tmesh_t netA = tmesh_new(meshA, NULL, test_init);
  fail_unless(netA);
  fail_unless(netA->path);
  LOG("netA %.*s",netA->path->head_len,netA->path->head);

  fail_unless(tmesh_sync(netA, "fzjb5f4tn4misgab7tb5rdrcay"));
  fail_unless(netA->syncs);
  fail_unless(netA->syncs->busy == 5000);
  
  fail_unless(tmesh_discoverable(netA, "kzdhpa5n6r4dtivft5ylvkr3di"));
  fail_unless(netA->disco);
  fail_unless(epochs_len(netA->disco->active) > 0);


  return 0;
}

