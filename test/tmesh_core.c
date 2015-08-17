#include "tmesh.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

uint32_t driver_get(mesh_t mesh, uint8_t medium[6])
{
  return 0;
}

void *driver_init(mesh_t mesh, epoch_t e, uint8_t medium[6])
{
  e->busy = 5000;
  return e;
}

epoch_t driver_free(mesh_t mesh, epoch_t e)
{
  return NULL;
}

static struct epoch_driver_struct test_driver = {
  driver_get,
  driver_init,
  driver_free
};

int main(int argc, char **argv)
{
  fail_unless(!e3x_init(NULL)); // random seed
  fail_unless(epoch_driver(&test_driver));
  
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = mesh_generate(meshA);
  fail_unless(secretsA);

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);
  
  tmesh_t netA = tmesh_new(meshA, NULL);
  fail_unless(netA);
  fail_unless(netA->path);
  LOG("netA %.*s",netA->path->head_len,netA->path->head);

  fail_unless(tmesh_sync(netA, "fzjb5f4tn4"));
  fail_unless(netA->syncs);
  fail_unless(netA->syncs->busy == 5000);
  
  fail_unless(tmesh_discoverable(netA, "kzdhpa5n6r", NULL));
  fail_unless(netA->disco);
  fail_unless(epochs_len(netA->disco) > 0);
  fail_unless(netA->dim);
  LOG("debug disco pkt %s",lob_json(netA->dim));
  


  return 0;
}

