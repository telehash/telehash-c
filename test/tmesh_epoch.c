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

  char mid[] = "fzjb5f4tn4";
  fail_unless(base32_decode(mid,0,medium,6));
  fail_unless((e = epoch_new(NULL,medium)));
  
  fail_unless(epoch_base(e,0,0));
  fail_unless(e->base == 0);
  fail_unless(!epoch_base(e,1,1));
  fail_unless(epoch_base(e,10,100000000));
  fail_unless(e->base == 58056960);
  fail_unless(e->base == (100000000 - (uint64_t)(10*(1<<22))));

  e->chans = 10;
  e->busy = 10;
  fail_unless(epoch_knock(e,1));
  fail_unless(e->knock);
  memset(e->secret,0,32);
  fail_unless(epoch_window(e,1));
  LOG("got channel %d start %d stop %d",e->knock->chan,e->knock->start,e->knock->stop);
  fail_unless(e->knock->chan == 6);
  fail_unless(e->knock->start == 62254984);
  fail_unless(e->knock->stop == 62254994);

  e->chans = 42;
  e->busy = 42;
  fail_unless(epoch_window(e,42));
  fail_unless(e->knock->chan == 18);
  fail_unless(e->knock->start == 234246960);
  fail_unless(e->knock->stop == 234247002);

  fail_unless(epoch_knock(e,EPOCH_WINDOW*4));
  LOG("got channel %d start %d stop %d",e->knock->chan,e->knock->start,e->knock->stop);
  fail_unless(e->knock->chan == 24);
  fail_unless(e->knock->start == 62266888);
  fail_unless(e->knock->stop == 62266930);

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

