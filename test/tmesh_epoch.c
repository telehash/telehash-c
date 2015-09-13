#include "tmesh.h"
#include "unit_test.h"

uint32_t device_check(mesh_t mesh, uint8_t medium[6])
{
  return 0;
}

medium_t device_get(mesh_t mesh, uint8_t medium[6])
{
  medium_t m;
  if(!(m = malloc(sizeof(struct medium_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct medium_struct));
  memcpy(m->bin,medium,6);
  m->chans = 100;
  m->min = 10;
  m->max = 1000;
  return m;
}

medium_t device_free(mesh_t mesh, medium_t m)
{
  return NULL;
}

static struct radio_struct test_device = {
  device_check,
  device_get,
  device_free,
  NULL
};

int main(int argc, char **argv)
{
  uint8_t bin[6];

  fail_unless(!e3x_init(NULL)); // random seed
  fail_unless(radio_device(&test_device));

//  epoch_t e = epoch_new(NULL,NULL);
//  fail_unless(!e);
  
  e3x_rand(bin,6);
  medium_t medium = radio_medium(NULL,bin);
//  e = epoch_new(NULL,medium);

  char mid[] = "fzjb5f4tn4";
  fail_unless(base32_decode(mid,0,bin,6));
  fail_unless((medium = radio_medium(NULL,bin)));
  /*
  fail_unless((e = epoch_new(NULL,medium)));
  
  fail_unless(epoch_base(e,0,0));
  fail_unless(e->base == 0);
  fail_unless(!epoch_base(e,1,1));
  fail_unless(epoch_base(e,10,100000000));
  fail_unless(e->base == 58056960);
  fail_unless(e->base == (100000000 - (uint64_t)(10*(1<<22))));

  fail_unless(epoch_knock(e,1));
  fail_unless(e->knock);
  memset(e->secret,0,32);
  fail_unless(epoch_window(e,1));
  LOG("got channel %d start %d stop %d",e->knock->chan,e->knock->start,e->knock->stop);
  fail_unless(e->knock->chan == 36);
  fail_unless(e->knock->start == 62254984);
  fail_unless(e->knock->stop == 62254994);

  fail_unless(epoch_window(e,42));
  LOG("got channel %d start %d stop %d",e->knock->chan,e->knock->start,e->knock->stop);
  fail_unless(e->knock->chan == 92);
  fail_unless(e->knock->start == 234224688);
  fail_unless(e->knock->stop == 234224698);

  fail_unless(epoch_knock(e,EPOCH_WINDOW*42));
  LOG("got channel %d start %d stop %d",e->knock->chan,e->knock->start,e->knock->stop);
  fail_unless(e->knock->chan == 48);
  fail_unless(e->knock->start == 179700976);
  fail_unless(e->knock->stop == 179700986);

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
  */
  return 0;
}

