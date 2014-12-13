#include "e3x.h"
#include "util.h"
#include "util_sys.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  uint64_t now = util_sys_ms(0);
  e3x_event_t ev = e3x_event_new(3, now);
  fail_unless(ev);
  fail_unless(e3x_event_at(ev, now));

  // TODO test the rest!

  return 0;
}

