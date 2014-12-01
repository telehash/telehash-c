#include "e3x.h"
#include "util.h"
#include "platform.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  uint64_t now = platform_ms(0);
  event3_t ev = event3_new(3, now);
  fail_unless(ev);
  fail_unless(event3_at(ev, now));

  // TODO test the rest!

  return 0;
}

