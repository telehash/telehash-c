#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  event3_t ev = event3_new(3);
  fail_unless(ev);
  fail_unless(event3_at(ev) == 0);

  // TODO test the rest!

  return 0;
}

