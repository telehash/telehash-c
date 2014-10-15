#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  channel3_t chan = channel3_new(NULL);
  fail_unless(!chan);

  // TODO all the things!

  return 0;
}

