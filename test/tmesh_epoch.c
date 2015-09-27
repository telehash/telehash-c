#include "tmesh.h"
#include "unit_test.h"

#include "./tmesh_device.c"
extern struct radio_struct test_device;

int main(int argc, char **argv)
{
  uint8_t bin[6];

  fail_unless(!e3x_init(NULL)); // random seed
  fail_unless(radio_device(&test_device));

  e3x_rand(bin,6);
  medium_t medium = medium_get(NULL,bin);
  fail_unless(medium);

  epoch_t e = epoch_new(0);
  fail_unless(e);

  char mid[] = "fzjb5f4tn4";
  fail_unless(base32_decode(mid,0,bin,6));
  fail_unless((medium = medium_get(NULL,bin)));
  
  fail_unless(epoch_base(e,0,0));
  fail_unless(e->base == 0);
  fail_unless(!epoch_base(e,1,1));
  fail_unless(epoch_base(e,10,100000000));
  fail_unless(e->base == 58056960);
  fail_unless(e->base == (100000000 - (uint64_t)(10*(1<<22))));

  mote_t mote = mote_new(NULL);
  fail_unless(mote);
  mote->epochs = e;
  memset(e->secret,0,32);
  e->base = 1;
  e->type = LINK;
  fail_unless(mote_knock(mote,medium,10));
  fail_unless(mote->knock == e);
  LOG("got channel %d start %d stop %d",mote->kchan,mote->kstart,mote->kstop);
  fail_unless(mote->kchan == 8);
  fail_unless(mote->kstart == 8961);
  fail_unless(mote->kstop == 8971);

  fail_unless(mote_knock(mote,medium,42*EPOCH_WINDOW));
  LOG("got channel %d start %d stop %d",mote->kchan,mote->kstart,mote->kstop);
  fail_unless(mote->kchan == 80);
  fail_unless(mote->kstart == 171974825);
  fail_unless(mote->kstop == 171974835);

  epoch_t es = epochs_add(NULL,e);
  fail_unless(es);
  fail_unless(epochs_len(es) == 1);
  fail_unless((es = epochs_add(es, e)));
  fail_unless(epochs_len(es) == 1);
  fail_unless(es == e);
  fail_unless(!(es = epochs_rem(es, e)));
  fail_unless(epochs_len(es) == 0);

  fail_unless((es = epochs_add(es, e)));
  fail_unless((es = epochs_add(es, epoch_new(1))));
  fail_unless(epochs_len(es) == 2);
  fail_unless((es = epochs_rem(es, e)));
  fail_unless(epochs_len(es) == 1);
  fail_unless(!(es = epochs_rem(es, es)));

  return 0;
}

