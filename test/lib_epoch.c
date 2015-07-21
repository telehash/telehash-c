#include "epoch.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  e3x_init(NULL); // random seed

  epoch_t e = epoch_new(NULL);
  fail_unless(e);
  fail_unless(epoch_id(e));
  fail_unless(epoch_reset(e));
  
  e3x_rand(e->bin,16);
  e->bin[15] = 0x1a; // for testing discovery epochs
  fail_unless(epoch_id(e));
  LOG("generated epoch %s",epoch_id(e));

  char eid[] = "fzjb5f4tn4misgab7tb5rdrcay";
  fail_unless(epoch_import(e,eid,NULL));
  fail_unless(e->bin[0] == 46);
  fail_unless(util_cmp(epoch_id(e),eid) == 0);
  fail_unless(epoch_import(e,NULL,eid));

  epoch_t es = epochs_add(NULL, e);
  fail_unless(es);
  fail_unless(epochs_len(es) == 1);
  fail_unless((es = epochs_add(es, e)));
  fail_unless(epochs_len(es) == 1);
  fail_unless(es == e);
  fail_unless(!(es = epochs_rem(es, e)));
  fail_unless(epochs_len(es) == 0);

  fail_unless((es = epochs_add(es, e)));
  fail_unless((es = epochs_add(es, epoch_new(NULL))));
  fail_unless(epochs_len(es) == 2);
  fail_unless((es = epochs_rem(es, e)));
  fail_unless(epochs_len(es) == 1);
  fail_unless(!(es = epochs_rem(es, es)));
  
  knock_t k = knock_new(0);
  fail_unless(k);
  fail_unless(epoch_knock(e, k, 1));
  fail_unless(!knock_free(k));
  
  return 0;
}

