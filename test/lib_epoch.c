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
  fail_unless(epoch_id(e));
  LOG("generated epoch %s",epoch_id(e));

  char eid[] = "fzjb5f4tn4misgab7tb5rdrcay";
  fail_unless(epoch_import(e,eid));
  fail_unless(e->type == 46);
  fail_unless(util_cmp(epoch_id(e),eid) == 0);

  return 0;
}

