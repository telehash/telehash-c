#include "epoch.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  epoch_t e = epoch_new(NULL);
  fail_unless(e);

  return 0;
}

