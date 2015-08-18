#include "tmesh.h"
#include "unit_test.h"

uint32_t device_get(mesh_t mesh, uint8_t medium[6])
{
  return 0;
}

void *device_bind(mesh_t mesh, epoch_t e, uint8_t medium[6])
{
  return (void*)e;
}

epoch_t device_free(mesh_t mesh, epoch_t e)
{
  return NULL;
}

static struct radio_struct test_device = {
  device_get,
  device_bind,
  device_free,
  NULL
};

int main(int argc, char **argv)
{
  uint8_t medium[6];

  fail_unless(!e3x_init(NULL)); // random seed
  fail_unless(radio_device(&test_device));

  epoch_t e = epoch_new(NULL,NULL);
  fail_unless(!e);
  
  e3x_rand(medium,6);
  e = epoch_new(NULL,medium);
  fail_unless(epoch_medium(e));
  LOG("generated epoch %s",epoch_medium(e));

  char mid[] = "fzjb5f4tn4";
  fail_unless(base32_decode(mid,0,medium,6));
  fail_unless((e = epoch_new(NULL,medium)));
  fail_unless(e->medium[0] == 46);
  fail_unless(util_cmp(epoch_medium(e),mid) == 0);

  epoch_t es = epochs_add(NULL, e);
  fail_unless(es);
  fail_unless(epochs_len(es) == 1);
  fail_unless((es = epochs_add(es, e)));
  fail_unless(epochs_len(es) == 1);
  fail_unless(es == e);
  fail_unless(!(es = epochs_rem(es, e)));
  fail_unless(epochs_len(es) == 0);

  fail_unless((es = epochs_add(es, e)));
  fail_unless((es = epochs_add(es, epoch_new(NULL,medium))));
  fail_unless(epochs_len(es) == 2);
  fail_unless((es = epochs_rem(es, e)));
  fail_unless(epochs_len(es) == 1);
  fail_unless(!(es = epochs_rem(es, es)));
    
  return 0;
}

